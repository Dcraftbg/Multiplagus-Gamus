#pragma once
#include <stdint.h>
typedef struct MapTile {
    // 0 means AIR. do not render
    unsigned short texture_id;
} MapTile;
typedef struct {
    size_t width, height;
    struct {
        Tile* items;
        size_t len, cap;
    } tiles;
} MapLayer;
