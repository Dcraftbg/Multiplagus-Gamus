#include <snet.h>
#include <assert.h>
#include <gt.h>
#include <stdlib.h>
#include "eprintf.h"
#include <list_head.h>
#include <packet.h>
#include <shared.h>
#include <shared.c>
#include <string.h>

#define PORT 8080
typedef struct {
    struct list_head list;
    int fd;
    uint32_t color;
    float x, y;
    uint32_t state;
    bool ready;
} Client;
static struct list_head clients = LIST_HEAD_INIT(clients);
static uint32_t game_state = GAME_STATE_LOBBY;
void send_packet(const Client* client, const Packet* packet) {
    gtblockfd(client->fd, GTBLOCKOUT);
    send(client->fd, packet, sizeof(*packet), 0);
}
void broadcast(const Client* ignore, const Packet* packet) {
    list_foreach(client_list, &clients) {
        Client* client = (Client*)client_list;
        if(client == ignore) continue;
        send_packet(client, packet);
    }
}

void update_client_state(Client* client){
    send_packet(client, &(Packet){
        .tag = SC_PACKET_CHANGE_YOUR_STATE,
        .as.sc_change_your_state.player_state = client->state,
    });
    broadcast(client, &(Packet) {
        .tag = SC_PACKET_CHANGE_SOMEONES_STATE,
        .as.sc_change_someones_state.id = client->fd,
        .as.sc_change_someones_state.player_state = client->state,
    });
}

#define SC_SOMEONE_JOINED(client) \
    { \
        .tag = SC_PACKET_SOMEONE_JOINED, \
        .as.sc_joined = { \
             .id = (client)->fd, \
             .color = (client)->color, \
             .x = (client)->x, \
             .y = (client)->y \
        } \
    }
void client_thread(void* client_void) {
    Client* client = client_void;
    for(;;) {
        Packet packet;
        gtblockfd(client->fd, GTBLOCKIN);
        int n = recv(client->fd, &packet, sizeof(packet), 0);
        if(n < 0) eprintfln("ERROR:%d: when receiving: %s", client->fd, sneterr());
        if(n <= 0) break;

        switch(packet.tag) {
        case CS_PACKET_HELLO:
            send_packet(client, &(Packet){
                .tag = SC_PACKET_CHANGE_GAME_STATE,
                .as.sc_change_game_state.game_state = game_state,
            });
            update_client_state(client);

            list_foreach(other_client_list, &clients) {
                Client* other_client = (Client*)other_client_list;
                send_packet(client, &(Packet) SC_SOMEONE_JOINED(other_client));
                send_packet(client, &(Packet) {
                    .tag = SC_PACKET_CHANGE_SOMEONES_STATE,
                    .as.sc_change_someones_state.id = other_client->fd,
                    .as.sc_change_someones_state.player_state = other_client->state,
                });
                if(game_state == GAME_STATE_LOBBY && other_client->ready) send_packet(client, &(Packet){
                    .tag = SC_PACKET_SOMEONES_READY,
                    .as.sc_someones_ready.id = other_client->fd,
                });
            }

            list_init(&client->list);
            list_append(&clients, &client->list);
            client->color = packet.as.cs_hello.color;
            eprintfln("INFO:%d: Hello", client->fd);
            broadcast(client, &(Packet) SC_SOMEONE_JOINED(client));
            break;
        case CS_PACKET_IM_HERE:
            client->x = packet.as.cs_here.x;
            client->y = packet.as.cs_here.y;
            eprintfln("INFO:%d: Here %f %f", client->fd, client->x, client->y);
            broadcast(client, &(Packet) {
                .tag = SC_PACKET_SOMEONE_HERE,
                .as.sc_here = {
                    .id = client->fd,
                    .x = client->x,
                    .y = client->y
                }
            });
            break;
        case CS_PACKET_READY: {
            client->ready = true;
            broadcast(client, &(Packet){
                .tag = SC_PACKET_SOMEONES_READY,
                .as.sc_someones_ready.id = client->fd,
            });
        } break;
        default:
            eprintfln("INFO:%d: Bogus Amogus", client->fd);
        }
    }
    broadcast(client, &(Packet) {
        .tag = SC_PACKET_SOMEONE_LEFT,
        .as.sc_left = {
            .id = client->fd
        }
    });
    eprintfln("INFO:%d: Closing", client->fd);
    closesocket(client->fd);
    list_remove(&client->list);
    free(client);
}

uint32_t invert_color(uint32_t color) {
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    r = 255 - r;
    g = 255 - g;
    b = 255 - b;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

struct {float x; float y;} initial_spawn_positions[] = {
    {
        200,200
    },
    {
        400,200
    },
    {
        200,300
    },
    {
        300,500
    },
    {
        425,225
    },
    {
        250,325
    },
    {
        320,550
    },
};

float first_zombie_timer = 0;
float match_timer = 0.0;
#define MATCH_DURATION 1.0;
#define TIME_TO_FIRST_ZOMBIE 1.0;
bool first_zombie_chosen = false;

void start_match(){
    size_t players_count = 0; // TODO: theres probably better way to do it
    list_foreach(client_list1,&clients){
        players_count++;
    }
    if(players_count < 2){
        eprintfln("You need at least 2 players rn there is %zu count", players_count);
        return;
    }

    //spawning players in random position
    list_foreach(client_list,&clients){
        Client* client = (Client*)client_list;
        size_t initial_spawn_position_index = rand() % sizeof(initial_spawn_positions)/sizeof(initial_spawn_positions[0]);
        client->x = initial_spawn_positions[initial_spawn_position_index].x;
        client->y = initial_spawn_positions[initial_spawn_position_index].y;
        send_packet(client, &(Packet){
            .tag = SC_PACKET_TELEPORT_HERE,
            .as.sc_teleport_here.x = client->x,
            .as.sc_teleport_here.y = client->y,
        });
        broadcast(client, &(Packet) {
            .tag = SC_PACKET_SOMEONE_HERE,
            .as.sc_here = {
                .id = client->fd,
                .x = client->x,
                .y = client->y
            }
        });
        client->state = PLAYER_STATE_HUMAN;
        update_client_state(client);
    }
    game_state = GAME_STATE_MATCH;
    first_zombie_timer = TIME_TO_FIRST_ZOMBIE;
    match_timer = MATCH_DURATION;
    first_zombie_chosen = false;
    broadcast(NULL, &(Packet){
        .tag = SC_PACKET_CHANGE_GAME_STATE,
        .as.sc_change_game_state = game_state,
    });
    broadcast(NULL, &(Packet){
        .tag = SC_PACKET_SET_TIMER,
        .as.sc_set_timer.duration = first_zombie_timer,
    });
    eprintfln("Match started");
}

void end_match(){
    game_state = GAME_STATE_LOBBY;
    broadcast(NULL, &(Packet){
        .tag = SC_PACKET_CHANGE_GAME_STATE,
        .as.sc_change_game_state = game_state,
    });
    list_foreach(client_list,&clients){
        Client* client = (Client*)client_list;
        client->state = PLAYER_STATE_SPECTATOR;
        client->ready = false;
        update_client_state(client);
    }
    first_zombie_timer = 0;
    match_timer = 0;
    first_zombie_chosen = false;
    eprintfln("Match ended");
}

void update_lobby(float dt){
    size_t connected_players_count = 0; // TODO: theres probably better way to do it
    size_t ready_count = 0;
    list_foreach(client_list,&clients) {
        Client* client = (Client*)client_list;
        connected_players_count++;
        if(client->ready) ready_count++;
    }
    if(connected_players_count < 2) return;
    if(ready_count == connected_players_count){
        start_match();
        return;
    }
}

void update_match(float dt){
    if(match_timer <= 0) {
        end_match();
        eprintfln("Timer run our humans won");
        return;
    }
    size_t connected_players_count = 0; // TODO: theres probably better way to do it
    size_t non_spectators_count = 0;
    size_t zombie_count = 0;
    list_foreach(client_list,&clients){
        Client* client = (Client*)client_list;
        connected_players_count++;
        if(client->state == PLAYER_STATE_ZOMBIE) zombie_count++;
        if(client->state != PLAYER_STATE_SPECTATOR) non_spectators_count++;
    }
    if(connected_players_count == 0) {
        end_match();
        eprintfln("No players in match");
        return;
    }
    if(non_spectators_count == zombie_count){
        end_match();
        eprintfln("Everyone turned into zombies");
        return;
    }

    bool choose_random_player_as_zombie = !first_zombie_chosen && first_zombie_timer <= 0;
    if(choose_random_player_as_zombie){
        size_t index = rand()%non_spectators_count;
        size_t cur_index = 0;
        list_foreach(client_list,&clients){
            Client* client = (Client*)client_list;
            if(client->state == PLAYER_STATE_SPECTATOR) continue;
            if(cur_index == index){
                client->state = PLAYER_STATE_ZOMBIE;
                first_zombie_chosen = true;
                update_client_state(client);
                broadcast(NULL, &(Packet){
                    .tag = SC_PACKET_SET_TIMER,
                    .as.sc_set_timer.duration = match_timer,
                });
                break;
            }
            cur_index++;
        }
    }

    //zombiefication
    list_foreach(client_list1,&clients){
        Client* client1 = (Client*)client_list1;
        if(client1->state == PLAYER_STATE_SPECTATOR) continue;
        list_foreach(client_list2,&clients){
            Client* client2 = (Client*)client_list2;
            if(client1 == client2) continue;
            if(client2->state == PLAYER_STATE_SPECTATOR) continue;

            bool collision = rects_colide(
                client1->x-PLAYER_SIZE/2, client1->y-PLAYER_SIZE/2,client1->x+PLAYER_SIZE/2,client1->y+PLAYER_SIZE/2,
                client2->x-PLAYER_SIZE/2, client2->y-PLAYER_SIZE/2,client2->x+PLAYER_SIZE/2,client2->y+PLAYER_SIZE/2
            );

            if(collision){
                if(client1->state == PLAYER_STATE_HUMAN && client2->state == PLAYER_STATE_ZOMBIE){
                    client1->state = PLAYER_STATE_ZOMBIE;
                    update_client_state(client1);
                }else if(client1->state == PLAYER_STATE_ZOMBIE && client2->state == PLAYER_STATE_HUMAN){
                    client2->state = PLAYER_STATE_ZOMBIE;
                    update_client_state(client2);
                }
            }
        }

    }

    if(first_zombie_timer > 0) first_zombie_timer -= dt;
    if(first_zombie_timer <= 0 && match_timer > 0) match_timer -= dt;
}

void update(void* ninja){
    (void)ninja;
    uint32_t now = gttime_now_unspec_milis();
    for(;;){
        uint32_t prev = now;
        now = gttime_now_unspec_milis();
        float dt = (now - prev) * .0001f;
        if(game_state == GAME_STATE_MATCH) update_match(dt);
        else if(game_state == GAME_STATE_LOBBY) update_lobby(dt);
        gtsleep(60);
    }
}

void console_input(void* ninja){
    (void)ninja;
    for(;;){
        gtblockfd(1, GTBLOCKIN);
        char buf[256];
        int has_read = read(1, buf, sizeof(buf));
        if(has_read <= 1) continue;
        buf[has_read-1] = 0;
        if(strcmp(buf, "start") == 0){
            start_match();
        }else if(strcmp(buf, "end") == 0){
            end_match();
        }else if(strcmp(buf, "zombie") == 0){
            list_foreach(client_list,&clients){
                Client* client = (Client*)client_list;
                if(client->state == PLAYER_STATE_SPECTATOR || client->state == PLAYER_STATE_ZOMBIE) continue;
                client->state = PLAYER_STATE_ZOMBIE;
                update_client_state(client);
                break;
            }
        }else{
            eprintfln("ERROR: unknown command %s", buf);
        }
    }
}

int main(void) {
    gtinit();
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if(server < 0) {
        eprintfln("FATAL: Could not create server socket: %s", sneterr());
        return 1;
    }
    int opt = 1;
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt)) < 0) {
        eprintfln("FATAL: Could not set SO_REUSEADDR: %s", sneterr());
        closesocket(server);
        return 1;
    }
    assert(server >= 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if(bind(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
        eprintfln("FATAL: Could not bind server: %s", sneterr());
        return 1;
    }
    if(listen(server, 64) < 0) {
        eprintfln("FATAL: Could not listen on server: %s", sneterr());
        return 1;
    }
    eprintfln("INFO: Listening on port 0.0.0.0:%d", PORT);
    gtgo(update, NULL);
    gtgo(console_input, NULL);
    for(;;) {
        Client* client = malloc(sizeof(Client));
        if(!client) {
            eprintfln("FATAL: Out of memory allocating client. Dying");
            break;
        }
        struct sockaddr_in client_addr = { 0 };
        unsigned int client_addr_len = sizeof(client_addr);
        gtblockfd(server, GTBLOCKIN);
        int client_fd = accept(server, (void*)&client_addr, &client_addr_len);
        client->fd = client_fd;
        eprintfln("Somebody connected!");
        gtgo(client_thread, client);
    }
    return 0;
}
