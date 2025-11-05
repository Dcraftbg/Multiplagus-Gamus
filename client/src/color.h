#pragma once
#include "vec.h"
// ARGB
typedef unsigned int Color;
#define COLOR_GET_A(c) (((c) >> 24) & 0xFF)
#define COLOR_GET_R(c) (((c) >> 16) & 0xFF)
#define COLOR_GET_G(c) (((c) >>  8) & 0xFF)
#define COLOR_GET_B(c) (((c) >>  0) & 0xFF)
static inline Vec4f rgb2vec4f(Color color) {
    Vec4f vec4 = {
        COLOR_GET_R(color) / 255.f,
        COLOR_GET_G(color) / 255.f,
        COLOR_GET_B(color) / 255.f,
        COLOR_GET_A(color) / 255.f
    };
    return vec4;
}
