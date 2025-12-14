#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <MsgPack.h>

//PC -> PICO
struct GcodeCom{
    uint8_t msgType;
    String Gcode;
    uint16_t length;

    MSGPACK_DEFINE(msgType, Gcode, length);
};

//PICO -> PC
struct MachineStatus{
    uint8_t msgType;
    String state;
    bool move_complete;
    bool ack;

    MSGPACK_DEFINE(msgType, state, move_complete, ack);
};


#endif
