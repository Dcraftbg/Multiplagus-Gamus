#include <shared.h>

bool rects_colide(
    float left_a,
    float top_a,
    float right_a,
    float bottom_a,

    float left_b,
    float top_b,
    float right_b,
    float bottom_b
){
    if (right_a < left_b || right_b < left_a)
        return false;

    if (bottom_a < top_b || bottom_b < top_a)
        return false;

    return true;
}