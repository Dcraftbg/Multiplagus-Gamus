#include <snet.h>
#include <assert.h>
#include <gt.h>
#include <stdlib.h>
#include "eprintf.h"
#include <list_head.h>
#include <packet.h>

#define PORT 8080
typedef struct {
    struct list_head list;
    int fd;
    uint32_t color;
    float x, y;
} Client;
static struct list_head clients = LIST_HEAD_INIT(clients);
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
            list_foreach(other_client_list, &clients) {
                Client* other_client = (Client*)other_client_list;
                send_packet(client, &(Packet) SC_SOMEONE_JOINED(other_client));
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
