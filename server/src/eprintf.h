#pragma once
#include <stdio.h>
#ifdef _WIN32
# define NEWLINE "\r\n"
#else
# define NEWLINE "\n"
#endif
#define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define eprintfln(fmt, ...) eprintf(fmt NEWLINE, ##__VA_ARGS__)
