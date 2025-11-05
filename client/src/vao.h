#pragma once
// An easy to use macro for binding float/vecx/matx
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
