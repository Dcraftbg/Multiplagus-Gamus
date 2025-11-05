#pragma once
#include "vec.h"
extern unsigned int debug_screen_width, debug_screen_height;
void debug_draw_triangle(float x0, float y0, float x1, float y1, float x2, float y2, Vec4f color);
void debug_draw_rect(float x, float y, float w, float h, Vec4f color);
void debug_draw_init(void);
void debug_batch_flush(void);

extern unsigned int debug_shader;
extern const char* debug_shader_vert;
extern const char* debug_shader_frag;
