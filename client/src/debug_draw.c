#include "debug_draw.h"
#include "opengl.h" 
#include "darray.h"
#include "vao.h"

typedef struct {
    Vec3f pos;
    Vec4f color;
    float texture_coords[2];
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
    unsigned int texture;
} debug_batch = { 0 };
unsigned int debug_shader = 0;
const char* debug_shader_vert = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 in_pos;\n"
    "layout (location = 1) in vec4 in_color;\n"
    "layout (location = 2) in vec2 in_texture_coords;\n"
    "out vec4 vert_color;\n"
    "out vec2 texture_coords;\n"
    "void main() {\n"
    "   gl_Position = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);\n"
    "   vert_color = in_color;\n"
    "   texture_coords = in_texture_coords;\n"
    "}\n";
const char* debug_shader_frag = 
    "#version 330 core\n"
    "in vec4 vert_color;\n"
    "in vec2 texture_coords;\n"
    "out vec4 out_color;\n"
    "uniform sampler2D sampler;\n"
    "void main() {\n"
    "   out_color = texture(sampler, texture_coords) * vert_color;\n"
    "}\n";
unsigned int debug_screen_width, debug_screen_height;
void debug_batch_flush(void) {
    glBindVertexArray(debug_batch.vao);
    glBindBuffer(GL_ARRAY_BUFFER, debug_batch.vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debug_batch.index_buffer);
    glBindTexture(GL_TEXTURE_2D, debug_batch.texture);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(debug_batch.shader);
    glBufferData(GL_ARRAY_BUFFER        , sizeof(*debug_batch.vertices.items) * debug_batch.vertices.len, debug_batch.vertices.items, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(*debug_batch.indecies.items) * debug_batch.indecies.len, debug_batch.indecies.items, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, debug_batch.indecies.len, GL_UNSIGNED_SHORT, 0);
    debug_batch.vertices.len = debug_batch.indecies.len = 0;
}
void debug_draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, Vec4f color) {
    debug_batch.shader = debug_shader;
    unsigned short start = debug_batch.vertices.len;
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { x0 / debug_screen_width * 2.0 - 1.0, y0 / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 0.0, 0.0 } }));
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { x1 / debug_screen_width * 2.0 - 1.0, y1 / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 1.0, 1.0 } }));
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { x2 / debug_screen_width * 2.0 - 1.0, y2 / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 1.0, 1.0 } }));
    for(size_t i = 0; i < 3; ++i) {
        unsigned short idx = start + i;
        da_push(&debug_batch.indecies, idx);
    }
}
void debug_draw_rect(float x, float y, float w, float h, Vec4f color) {
    debug_batch.shader = debug_shader;
    unsigned short start = debug_batch.vertices.len;
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { (x    ) / debug_screen_width * 2.0 - 1.0, (y    ) / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 0.0, 0.0 } }));
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { (x + w) / debug_screen_width * 2.0 - 1.0, (y    ) / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 1.0, 0.0 } }));
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { (x    ) / debug_screen_width * 2.0 - 1.0, (y + h) / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 0.0, 1.0 } }));
    da_push(&debug_batch.vertices, ((BatchVertex){ .pos = { (x + w) / debug_screen_width * 2.0 - 1.0, (y + h) / debug_screen_height * 2.0 - 1.0, 1.0 }, .color = color, .texture_coords = { 1.0, 1.0 } }));
    const unsigned short indecies[] = {
        0, 1, 3,
        0, 2, 3
    };
    for(size_t i = 0; i < sizeof(indecies)/sizeof(*indecies); ++i) {
        unsigned short idx = start + indecies[i];
        da_push(&debug_batch.indecies, idx);
    }
}
void debug_draw_init(void) {
    glGenVertexArrays(1, &debug_batch.vao);
    glBindVertexArray(debug_batch.vao);
    glGenBuffers(1, &debug_batch.index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, debug_batch.index_buffer);
    glGenBuffers(1, &debug_batch.vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, debug_batch.vertex_buffer);

    // Shape texture
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    uint32_t white = 0xFFFFFFFF;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
    // Shape texture
    debug_batch.texture = texture;
    unsigned int loc = 0;
    vao_bind_float(loc++, BatchVertex, pos);
    vao_bind_float(loc++, BatchVertex, color);
    vao_bind_float(loc++, BatchVertex, texture_coords);
}
