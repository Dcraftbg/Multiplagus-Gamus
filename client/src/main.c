#include "RGFW.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include "darray.h"
#include <snet.h>
#include <assert.h>
#include <gt.h>
#include <packet.h>
#include <math.h>

#define W_RATIO 16
#define H_RATIO 9
#define SCALE 70
#define WIDTH (W_RATIO*SCALE)
#define HEIGHT (H_RATIO*SCALE)

typedef union {
    struct {
        float r, g, b, a;
    };
    struct {
        float x, y, z, w;
    };
    float f[4];
} Vec4f;
// ARGB
typedef unsigned int Color;
#define COLOR_GET_A(c) (((c) >> 24) & 0xFF)
#define COLOR_GET_R(c) (((c) >> 16) & 0xFF)
#define COLOR_GET_G(c) (((c) >>  8) & 0xFF)
#define COLOR_GET_B(c) (((c) >>  0) & 0xFF)
Vec4f rgb2vec4f(Color color) {
    Vec4f vec4 = {
        COLOR_GET_R(color) / 255.f,
        COLOR_GET_G(color) / 255.f,
        COLOR_GET_B(color) / 255.f,
        COLOR_GET_A(color) / 255.f
    };
    return vec4;
}
typedef struct {
    float x, y, z;
} Vec3f;
typedef struct {
    Vec3f pos;
    Vec4f color;
} BatchVertex;
static struct {
    struct {
        BatchVertex *items;
        size_t len, cap;
    } vertices;
    struct {
        unsigned short *items;
        size_t len, cap;
    } indecies;
    unsigned int vertex_buffer, 
                 index_buffer,
                 vao;
    unsigned int shader;
} triangle_batch = { 0 };
static void batch_flush(void) {
    glBindVertexArray(triangle_batch.vao);
    glUseProgram(triangle_batch.shader);
    glBufferData(GL_ARRAY_BUFFER        , sizeof(*triangle_batch.vertices.items) * triangle_batch.vertices.len, triangle_batch.vertices.items, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*triangle_batch.indecies.items) * triangle_batch.indecies.len, triangle_batch.indecies.items, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, triangle_batch.indecies.len, GL_UNSIGNED_SHORT, 0);
    triangle_batch.vertices.len = triangle_batch.indecies.len = 0;
}
static unsigned int compile_shader_kind_or_crash(const char* src, unsigned int kind) {
    unsigned int shader = glCreateShader(kind);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    char log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, log);
        fprintf(stderr, "Failed to compile %s shader:\n%s\n", kind == GL_VERTEX_SHADER ? "vertex" : kind == GL_FRAGMENT_SHADER ? "fragment" : "other", log);
        exit(1);
    }
    return shader;
}
static unsigned int compile_shader_or_crash(const char* vert_src, const char* frag_src) {
    unsigned int program = glCreateProgram();
    unsigned int vert = compile_shader_kind_or_crash(vert_src, GL_VERTEX_SHADER);
    unsigned int frag = compile_shader_kind_or_crash(frag_src, GL_FRAGMENT_SHADER);
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    int success;
    char log[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, log);
        fprintf(stderr, "Failed to link shader:\n%s\n", log);
        exit(1);
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    if(!success) exit(1);
    return program;
}
static unsigned int debug_shader = 0;
const char* debug_shader_vert = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 in_pos;\n"
    "layout (location = 1) in vec4 in_color;\n"
    "out vec4 vert_color;\n"
    "void main() {\n"
    "   gl_Position = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);\n"
    "   vert_color = in_color;\n"
    "}\n";
const char* debug_shader_frag = 
    "#version 330 core\n"
    "in vec4 vert_color;\n"
    "out vec4 out_color;\n"
    "void main() {\n"
    "   out_color = vert_color;\n"
    "}\n";

static unsigned int screen_width = WIDTH, screen_height = HEIGHT;
#define vao_bind_float(loc, type, field) \
    do { \
        unsigned int __loc = (loc); \
        glVertexAttribPointer(__loc, \
                sizeof(((type*)0)->field) / sizeof(float), \
                GL_FLOAT, \
                GL_FALSE, \
                sizeof(type), \
                (void*)offsetof(type, field)); \
        glEnableVertexAttribArray(__loc); \
    } while(0)

void debug_draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, Vec4f color) {
    triangle_batch.shader = debug_shader;
    unsigned short start = triangle_batch.vertices.len;
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { x0 / screen_width * 2.0 - 1.0, y0 / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { x1 / screen_width * 2.0 - 1.0, y1 / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { x2 / screen_width * 2.0 - 1.0, y2 / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    for(size_t i = 0; i < 3; ++i) {
        unsigned short idx = start + i;
        da_push(&triangle_batch.indecies, idx);
    }
}
void debug_draw_rect(float x, float y, float w, float h, Vec4f color) {
    triangle_batch.shader = debug_shader;
    unsigned short start = triangle_batch.vertices.len;
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { (x    ) / screen_width * 2.0 - 1.0, (y    ) / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { (x + w) / screen_width * 2.0 - 1.0, (y    ) / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { (x    ) / screen_width * 2.0 - 1.0, (y + h) / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    da_push(&triangle_batch.vertices, ((BatchVertex){ .pos = { (x + w) / screen_width * 2.0 - 1.0, (y + h) / screen_height * 2.0 - 1.0, 1.0 }, .color = color }));
    const unsigned short indecies[] = {
        0, 1, 3,
        0, 2, 3
    };
    for(size_t i = 0; i < sizeof(indecies)/sizeof(*indecies); ++i) {
        unsigned short idx = start + indecies[i];
        da_push(&triangle_batch.indecies, idx);
    }
}
typedef struct {
    uint32_t id;
    uint32_t color;
    float target_x, target_y;
    float x, y;
} Player;
typedef struct {
    Player* items;
    size_t len, cap;
} Players;

static Players players = { 0 };
int fd;
void send_packet(const Packet* packet) {
    gtblockfd(fd, GTBLOCKOUT);
    send(fd, packet, sizeof(*packet), 0);
}
size_t lookup_player_idx(uint32_t id) {
    for(size_t i = 1; i < players.len; ++i) {
        if(players.items[i].id == id) return i;
    }
    return 0;
}
void net_thread(void* ninja) {
    (void)ninja;
    for(;;) {
        Packet packet = { 0 };
        gtblockfd(fd, GTBLOCKIN);
        recv(fd, &packet, sizeof(packet), 0);
        switch(packet.tag) {
        case SC_PACKET_SOMEONE_JOINED: {
            fprintf(stderr, "Someone joined %d!\n", packet.as.sc_joined.id);
            Player player = {
                .id = packet.as.sc_joined.id,
                .color = packet.as.sc_joined.color,
                .x = packet.as.sc_joined.x,
                .y = packet.as.sc_joined.y,
                .target_x = packet.as.sc_joined.x,
                .target_y = packet.as.sc_joined.y,
            };
            da_push(&players, player);
        } break;
        case SC_PACKET_SOMEONE_HERE: {
            size_t player_idx = lookup_player_idx(packet.as.sc_here.id);
            if(player_idx) {
                Player* player = &players.items[player_idx];
                player->target_x = packet.as.sc_here.x;
                player->target_y = packet.as.sc_here.y;
            } else {
                fprintf(stderr, "Bogus here %d!\n", packet.as.sc_here.id);
            }
        } break;
        case SC_PACKET_SOMEONE_LEFT: {
            size_t idx = lookup_player_idx(packet.as.sc_left.id);
            if(idx) da_remove_unordered(&players, idx);
            else {
                fprintf(stderr, "Bogus left %d!\n", packet.as.sc_left.id);
            }
        } break;
        default:
            fprintf(stderr, "Bogus packet!\n");
            break;
        }
    }
}
char* shift_args(int *argc, char ***argv) {
    if((*argc) <= 0) return NULL;
    return ((*argc)--, *((*argv)++));
}

float exp_decayf(float a, float b, float decay, float deltaTime){
    return b + (a - b) * expf(-decay*deltaTime);
}

int main(int argc, char** argv) {
    gtinit();
    shift_args(&argc, &argv);
    uint32_t color = 0xffff0000;
    const char* hostname = "localhost";
    unsigned short port = 8080;
    while(argc) {
        char* arg = shift_args(&argc, &argv);
        if(strcmp(arg, "--color") == 0) {
            assert(argc && "Color mofo");
            color = strtoll(shift_args(&argc, &argv), NULL, 16);
            if(color == 0) color = 0xffff0000;
            color |= 0xff000000;
        } else if(strcmp(arg, "-p") == 0) {
            assert(argc && "port");
            port = strtoll(shift_args(&argc, &argv), NULL, 10);
        } else if(strcmp(arg, "-ip") == 0) {
            assert(argc && "ip");
            hostname = shift_args(&argc, &argv);
        } else {
            fprintf(stderr, "Arg: %s\n", arg);
            return 0;
        }
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if(inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(hostname);
        if (he == NULL) {
            fprintf(stderr, "FATAL: Couldn't resolve hostname %s: %s\n", hostname, sneterr()); 
            return 1;
        }
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    fd = socket(AF_INET, SOCK_STREAM, 0); 
    assert(fd >= 0);
    int opt = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        fprintf(stderr, "FATAL: Could not set SO_REUSEADDR: %s\n", sneterr());
        closesocket(fd);
        return 1;
    }
    if(connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "FATAL: Couldn't connect to %s %d: %s\n", hostname, port, sneterr());
        return 1;
    }
    send_packet(&(Packet) {
        .tag = CS_PACKET_HELLO,
        .as.cs_hello = {
            .color = color,
        }
    });
    gtgo(net_thread, NULL);
    RGFW_window* win = RGFW_createWindow(
                    "Client",
                    0, 0,
                    WIDTH, HEIGHT,
                    RGFW_windowCenter | RGFW_windowNoResize | RGFW_windowOpenGL);
    glGenVertexArrays(1, &triangle_batch.vao);
    glBindVertexArray(triangle_batch.vao);
    glGenBuffers(1, &triangle_batch.index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, triangle_batch.index_buffer);

    glGenBuffers(1, &triangle_batch.vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, triangle_batch.vertex_buffer);
    debug_shader = compile_shader_or_crash(debug_shader_vert, debug_shader_frag);

    unsigned int loc = 0;
    vao_bind_float(loc++, BatchVertex, pos);
    vao_bind_float(loc++, BatchVertex, color);
    da_push(&players, ((Player) { .color = color }));

    float our_velocity_x = 0, our_velocity_y = 0;
    uint32_t now = gttime_now_unspec_milis();
    while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
        uint32_t prev = now;
        now = gttime_now_unspec_milis();
        float dt = (now - prev) * .0001f;
        RGFW_event event;
        while (RGFW_window_checkEvent(win, &event));
        int dx = 0, dy = 0;
        if(RGFW_isKeyDown(RGFW_left)) dx -= 1;
        if(RGFW_isKeyDown(RGFW_right)) dx += 1;
        if(RGFW_isKeyDown(RGFW_up)) dy += 1;
        if(RGFW_isKeyDown(RGFW_down)) dy -= 1;
        our_velocity_y += dy * 2.0;
        our_velocity_x += dx;
        our_velocity_x *= 0.8f;

        float our_old_x = players.items[0].x,
              our_old_y = players.items[0].y;

        players.items[0].x += our_velocity_x;
        players.items[0].y += our_velocity_y;
        if(players.items[0].y < 0) {
            players.items[0].y = 0;
            our_velocity_y = 0.0;
        } else {
            our_velocity_y -= 1;
        }
        if(fabsf(our_old_x - players.items[0].x) >= 0.0001 ||
           fabsf(our_old_y - players.items[0].y) >= 0.0001
        ) {
            send_packet(&(Packet) {
                .tag = CS_PACKET_IM_HERE,
                .as.cs_here = {
                    .x = players.items[0].x,
                    .y = players.items[0].y
                }
            });
        }
        glClearColor(0x21/255.f, 0x21/255.f, 0x21/255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        for(size_t i = 1; i < players.len; ++i) { // interpolating
            Player* player = &players.items[i];
            player->x = exp_decayf(player->x, player->target_x, 1000, dt); 
            player->y = exp_decayf(player->y, player->target_y, 1000, dt); 
        }
        for(size_t i = 0; i < players.len; ++i) {
            debug_draw_rect(players.items[i].x, players.items[i].y, 32, 32, rgb2vec4f(players.items[i].color));
        }
        batch_flush();
        glFlush();
        RGFW_window_swapBuffers_OpenGL(win);
        gtyield();
    }
    RGFW_window_close(win);
    return 0;
}
