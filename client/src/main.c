#include "RGFW.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include "darray.h"
#include <snet.h>
#include <assert.h>
#include <gt.h>
#include <packet.h>
#include <math.h>
#include <stb_image.h>
#include "debug_draw.h"
#include "color.h"
#include "vao.h"

#define W_RATIO 16
#define H_RATIO 9
#define SCALE 70
#define WIDTH (W_RATIO*SCALE)
#define HEIGHT (H_RATIO*SCALE)

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
int create_opengl_texture_from_file(const char* path) {
    int width, height, numChannels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* texture_data = stbi_load(path, &width, &height, &numChannels, 4);
    if(!texture_data) {
        fprintf(stderr, "ERROR loading texture \"%s\": Failed to load texture\n", path);
        return -1;
    }
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture_data);
    stbi_image_free(texture_data);
    return texture;
}

#define CHUNK_WIDTH  16
#define CHUNK_HEIGHT 16
#define STRINGIFY0(x) # x
#define STRINGIFY1(x) STRINGIFY0(x)
static_assert(CHUNK_WIDTH == 16 && CHUNK_HEIGHT == 16, "Update tile mask in shader");
static unsigned int tile_shader = 0;
#define TILE_UNIFORMS(X) \
    X(tile_scale) \
    X(tile_texture_width) \
    X(chunk_tiles) \
    X(chunk_pos)

#define UNIFORM_DECLARE(name) \
    unsigned int name;
struct {
TILE_UNIFORMS(UNIFORM_DECLARE);
} tile_uniforms;

#define TILE_UNIFORM_LOAD(name) \
    tile_uniforms.name = glGetUniformLocation(tile_shader, #name);
void load_tile_uniforms(void) {
TILE_UNIFORMS(TILE_UNIFORM_LOAD);
}
typedef struct TileVertex {
    float uv[2];
} TileVertex;
static unsigned int tile_vao, tile_vbo, tile_veo;
void tile_init_buffers(void) {
    glGenVertexArrays(1, &tile_vao);
    glBindVertexArray(tile_vao);
    glGenBuffers(1, &tile_veo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tile_veo);
    glGenBuffers(1, &tile_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, tile_vbo);
    unsigned int loc = 0;
    vao_bind_float(loc++, TileVertex, uv);
    TileVertex verticies[] = {
        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        1.0, 1.0
    };
    const unsigned char indecies[] = {
        0, 1, 3,
        0, 2, 3
    };
    glBufferData(GL_ARRAY_BUFFER        , sizeof(verticies), verticies, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indecies), indecies, GL_STATIC_DRAW);
}
static const char* tile_vert = 
    "#version 330 core\n"
    "in vec2 vert_in_uv;\n"
    "out vec2 frag_in_uv;\n"
    "flat out uint tile_texture_id;\n"
    "flat out float frag_tile_texture_width;\n"
    "uniform float tile_texture_width;\n"
    "uniform vec2 tile_scale;\n"
    "uniform uint chunk_tiles[" STRINGIFY1(CHUNK_WIDTH*CHUNK_HEIGHT) "];\n"
    "uniform vec2 chunk_pos;\n"
    "void main() {\n"
    // Unpacking the tile
    "   uint tile = chunk_tiles[gl_InstanceID];"
    "   uint local_x = uint((int(tile) >> 0) & 0xF);\n"
    "   uint local_y = uint((int(tile) >> 4) & 0xF);\n"
    "   tile_texture_id = uint((int(tile) >> 8) & 0xFFFF);\n"
    // Passing fragment stuff
    "   frag_in_uv = vert_in_uv;\n"
    "   frag_tile_texture_width = tile_texture_width;\n"
    "   vec2 xy = vec2(float(local_x), float(local_y));\n"
    "   gl_Position = vec4((chunk_pos + ((vert_in_uv + xy) * tile_scale)) * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";
static const char* tile_frag = 
    "#version 330 core\n"
    "in vec2 frag_in_uv;\n"
    "flat in uint tile_texture_id;\n"
    "flat in float frag_tile_texture_width;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D sampler;\n"
    "void main() {\n"
    "   vec2 texture_coords = vec2(\n"
                "(float(tile_texture_id) + frag_in_uv.x) * frag_tile_texture_width,\n"
                "frag_in_uv.y);\n"
    "   frag_color = texture(sampler, texture_coords);\n"
    "}\n";
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
#define BAKED_MAP_WIDTH 15*3
#define BAKED_MAP_HEIGHT 11
static unsigned short baked_map[BAKED_MAP_WIDTH*BAKED_MAP_HEIGHT] = {
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2
};
#define CHUNK_PACK(texture, local_x, local_y) ((texture) << 8) | ((local_y) << 4) | ((local_x) << 0)
void render_map(float camera_x, float camera_y, unsigned int atlas, unsigned short* map, size_t map_width, size_t map_height) {
    for(size_t y = 0; y < map_height; y += CHUNK_HEIGHT) {
        for(size_t x = 0; x < map_width; x += CHUNK_WIDTH) {
            unsigned int chunk[CHUNK_WIDTH*CHUNK_HEIGHT];
            unsigned int chunk_n = 0;
            for(size_t local_y = 0; local_y < CHUNK_HEIGHT && local_y < (map_height-y); local_y++) {
                for(size_t local_x = 0; local_x < CHUNK_WIDTH && local_x < (map_width-x); local_x++) {
                    unsigned short tile = map[(y + local_y) * map_width + (x + local_x)];
                    if(tile) {
                        chunk[chunk_n++] = CHUNK_PACK(tile - 1, local_x, local_y);
                    }
                }
            }
            if(chunk_n) {
                // TODO: unhardcode this :)
                float tile_width = 64.f / WIDTH, 
                      tile_height = 64.f / HEIGHT;

                glUseProgram(tile_shader);
                glBindTexture(GL_TEXTURE_2D, atlas);
                glActiveTexture(GL_TEXTURE0);
                glBindVertexArray(tile_vao);
                glBindBuffer(GL_ARRAY_BUFFER, tile_vbo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tile_veo);

                glUniform1uiv(tile_uniforms.chunk_tiles, chunk_n, chunk);
                glUniform2f(tile_uniforms.chunk_pos, tile_width * (x) - camera_x/WIDTH, tile_height * (y) - camera_y/HEIGHT);
                glUniform2f(tile_uniforms.tile_scale, tile_width, tile_height);

                glUniform1f(tile_uniforms.tile_texture_width, 32.0/64.0);
                // glDrawArraysInstanced(GL_TRIANGLES, 0, 6, chunk_n);
                glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, NULL, chunk_n);
            }
        }
    }
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

    debug_draw_init();
    tile_init_buffers();
    debug_screen_width = WIDTH;
    debug_screen_height = HEIGHT;
#define SHADER(name, vert, frag) \
    printf("Compiling %s shader...\n", # name); \
    name##_shader = compile_shader_or_crash(vert, frag);

    printf("Compiling shaders...\n");
    SHADER(debug, debug_shader_vert, debug_shader_frag);
    SHADER(tile, tile_vert, tile_frag);
    load_tile_uniforms();
    printf("Compiled shaders!\n");

#define TEXTURE(name, path) \
    printf("Loading %s (%s)...\n", #name, path); \
    name = create_opengl_texture_from_file(path); \
    if((int)name < 0) { \
        fprintf(stderr, "Failed to load %s\n", #name); \
        return 1; \
    } \
    glGenerateMipmap(GL_TEXTURE_2D);

    unsigned int map_atlas = 0;
    printf("Loading textures...\n");
    TEXTURE(map_atlas, "floor.png");
    printf("Loaded textures!\n");
    da_push(&players, ((Player) { .color = color }));

    // glActiveTexture(GL_TEXTURE0);
    float our_velocity_x = 0, our_velocity_y = 0;
    uint32_t now = gttime_now_unspec_milis();

    float camera_x = 0, camera_y = 0;
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

        float our_old_x = players.items[0].x,
              our_old_y = players.items[0].y;

        players.items[0].x += dx * 3000 * dt;
        players.items[0].y += dy * 3000 * dt;
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
        float camera_target_x = players.items[0].x + 32 - (WIDTH/2),
              camera_target_y = players.items[0].y + 32 - (HEIGHT/2);
        if(fabsf(camera_x - camera_target_x) <= 0.0001) {
            camera_x = camera_target_x;
        }
        if(fabsf(camera_y - camera_target_y) <= 0.0001) {
            camera_y = camera_target_y;
        }
        camera_x += (camera_target_x - camera_x) * 1000 * dt;
        camera_y += (camera_target_y - camera_y) * 1000 * dt;
        glClearColor(0x21/255.f, 0x21/255.f, 0x21/255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        render_map(camera_x, camera_y, map_atlas, baked_map, BAKED_MAP_WIDTH, BAKED_MAP_HEIGHT);
        for(size_t i = 1; i < players.len; ++i) { // interpolating
            Player* player = &players.items[i];
            player->x = exp_decayf(player->x, player->target_x, 1000, dt); 
            player->y = exp_decayf(player->y, player->target_y, 1000, dt); 
        }
        for(size_t i = 0; i < players.len; ++i) {
            debug_draw_rect(players.items[i].x - camera_x, players.items[i].y - camera_y, 32, 32, rgb2vec4f(players.items[i].color));
        }
        debug_batch_flush();
        glFlush();
        RGFW_window_swapBuffers_OpenGL(win);
        gtyield();
    }
    RGFW_window_close(win);
    return 0;
}
