#include "RGFW.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include "darray.h"
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
    float x, y;
    float velocity_x, velocity_y;
} Player;
typedef struct {
    Player* items;
    size_t len, cap;
} Players;
int main(void) {
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
    Players players = { 0 };
    da_push(&players, ((Player) {0}));
    da_push(&players, ((Player) {0}));

    while (RGFW_window_shouldClose(win) == RGFW_FALSE) {
        RGFW_event event;
        while (RGFW_window_checkEvent(win, &event));
        int dx = 0, dy = 0;
        if(RGFW_isKeyDown(RGFW_left)) dx -= 1;
        if(RGFW_isKeyDown(RGFW_right)) dx += 1;
        if(RGFW_isKeyDown(RGFW_up)) dy += 1;
        if(RGFW_isKeyDown(RGFW_down)) dy -= 1;
        players.items[0].velocity_y += dy * 2.0;
        players.items[0].velocity_x += dx;
        players.items[0].velocity_x *= 0.8f;

        players.items[0].x += players.items[0].velocity_x;
        players.items[0].y += players.items[0].velocity_y;
        if(players.items[0].y < 0) {
            players.items[0].y = 0;
            players.items[0].velocity_y = 0.0;
        } else {
            players.items[0].velocity_y -= 1;
        }
        glClearColor(0x21/255.f, 0x21/255.f, 0x21/255.f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        for(size_t i = 0; i < players.len; ++i) {
            debug_draw_rect(players.items[i].x, players.items[i].y, 32, 32, rgb2vec4f(0xff00ff00));
        }
        batch_flush();
        glFlush();
        RGFW_window_swapBuffers_OpenGL(win);
    }
    RGFW_window_close(win);
    return 0;
}
