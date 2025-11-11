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
typedef struct ScTeleportHere{
    float x, y;
} ScTeleportHere;
typedef struct ScSomeoneLeft {
    uint32_t id;
} ScSomeoneLeft;
typedef struct ScChangeGameState {
    uint32_t game_state;
} ScChangeGameState;
typedef struct ScChangeYourState {
    uint32_t player_state;
} ScChangeYourState;
typedef struct ScChangeSomeonesState {
    uint32_t id;
    uint32_t player_state;
} ScChangeSomeonesState;
typedef struct ScSetTimer{
    float duration;
} ScSetTimer;
typedef struct ScSomeonesReady{
    uint32_t id;
} ScSomeonesReady;
enum {
    CS_PACKET_HELLO,
    CS_PACKET_IM_HERE,
    CS_PACKET_READY,
    CS_PACKET_COUNT
};
enum {
    SC_PACKET_SOMEONE_JOINED,
    SC_PACKET_SOMEONE_HERE,
    SC_PACKET_SOMEONE_LEFT,
    SC_PACKET_CHANGE_GAME_STATE,
    SC_PACKET_TELEPORT_HERE,
    SC_PACKET_CHANGE_YOUR_STATE,
    SC_PACKET_CHANGE_SOMEONES_STATE,
    SC_PACKET_SET_TIMER,
    SC_PACKET_SOMEONES_READY,
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
        ScChangeGameState sc_change_game_state;
        ScTeleportHere sc_teleport_here;
        ScChangeYourState sc_change_your_state;
        ScChangeSomeonesState sc_change_someones_state;
        ScSomeonesReady sc_someones_ready;
        ScSetTimer sc_set_timer;
    } as;
} Packet;
