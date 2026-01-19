#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <MsgPack.h>

// Zwiększamy rozmiar bufora GCode, aby uniknąć ucinania długich komend
#define MAX_GCODE_LEN 64 
#define STATE_LEN 20

struct GcodeCom{
    uint8_t msgType; 
    char Gcode[MAX_GCODE_LEN]; 
    int id;
    bool is_last; 
    
    void to_msgpack(MsgPack::Packer& packer) const {
        packer.pack(msgType);
        packer.pack(Gcode); 
        packer.pack(id);
        packer.pack(is_last);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) {
        unpacker.unpack(msgType);
        String temp;
        unpacker.unpack(temp);
        strncpy(Gcode, temp.c_str(), MAX_GCODE_LEN);
        Gcode[MAX_GCODE_LEN - 1] = '\0'; 
        unpacker.unpack(id);
        unpacker.unpack(is_last);
    }
};

struct MachineStatus{
    uint8_t msgType; 
    char state[STATE_LEN]; 
    int id; 
    bool ack; 
    
    void to_msgpack(MsgPack::Packer& packer) const {
        packer.pack(msgType);
        packer.pack(state);
        packer.pack(id);
        packer.pack(ack);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) {
        unpacker.unpack(msgType);
        String temp;
        unpacker.unpack(temp);
        strncpy(state, temp.c_str(), STATE_LEN);
        state[STATE_LEN - 1] = '\0';
        unpacker.unpack(id);
        unpacker.unpack(ack);
    }
};

struct processedStatus{
    uint8_t msgType; 
    char state[STATE_LEN];
    int id; 
    bool ack; 
    uint8_t target_client;   
};

struct processedData{
    uint8_t msgType; 
    char Gcode[MAX_GCODE_LEN];
    int id;
    bool is_last; 
    uint8_t client_num;
};

#endif