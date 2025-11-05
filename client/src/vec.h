#pragma once
typedef union {
    struct {
        float r, g, b, a;
    };
    struct {
        float x, y, z, w;
    };
    float f[4];
} Vec4f;
typedef struct {
    float x, y, z;
} Vec3f;
