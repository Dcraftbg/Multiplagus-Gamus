#pragma once

#define PLAYER_SIZE 32

#include <stdbool.h>
bool rects_colide(
    float left_a,
    float top_a,
    float right_a,
    float bottom_a,

    float left_b,
    float top_b,
    float right_b,
    float bottom_b
);