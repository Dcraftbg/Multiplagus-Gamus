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

enum {
    GAME_STATE_NONE = 0,
    GAME_STATE_LOBBY,
    GAME_STATE_MATCH,
};

enum {
    PLAYER_STATE_SPECTATOR = 0,
    PLAYER_STATE_HUMAN,
    PLAYER_STATE_ZOMBIE,
};