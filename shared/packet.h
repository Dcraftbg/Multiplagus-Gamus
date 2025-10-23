#pragma once
#include <stdio.h>
typedef struct CsHello {
    uint32_t color;
} CsHello;
typedef struct CsImHere {
    float x, y;
} CsImHere;
typedef struct ScSomeoneJoined {
    uint32_t id;
    uint32_t color;
    float x, y;
} ScSomeoneJoined;
typedef struct ScSomeoneHere {
    uint32_t id;
    float x, y;
} ScSomeoneHere;
typedef struct ScSomeoneLeft {
    uint32_t id;
} ScSomeoneLeft;
enum {
    CS_PACKET_HELLO,
    CS_PACKET_IM_HERE,
    CS_PACKET_COUNT
};
enum {
    SC_PACKET_SOMEONE_JOINED,
    SC_PACKET_SOMEONE_HERE,
    SC_PACKET_SOMEONE_LEFT,
    SC_PACKET_COUNT
};
typedef struct Packet {
    uint16_t tag;
    union {
        CsHello cs_hello;
        CsImHere cs_here;
        ScSomeoneJoined sc_joined;
        ScSomeoneHere sc_here;
        ScSomeoneLeft sc_left;
    } as;
} Packet;
