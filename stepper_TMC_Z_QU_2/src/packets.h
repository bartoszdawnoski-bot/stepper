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
    
    void to_msgpack(MsgPack::Packer& packer) const 
    {
        packer.pack(arduino::msgpack::arr_size_t(4));
        packer.pack(msgType);
        packer.pack(Gcode); 
        packer.pack(id);
        packer.pack(is_last);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) 
    {
        arduino::msgpack::arr_size_t sz;
        if (unpacker.unpack(sz)) 
        {
            unpacker.unpack(msgType);
            static std::vector<char> temp;
            temp.clear(); 
            if(temp.capacity() < MAX_GCODE_LEN + 1)
            {
                temp.reserve(MAX_GCODE_LEN + 16);
            }
            unpacker.unpack(temp); 
            size_t len = temp.size();
            if(len >= MAX_GCODE_LEN) len = MAX_GCODE_LEN - 1;
            if (len > 0) memcpy(Gcode, temp.data(), len);
            Gcode[len] = '\0';
            unpacker.unpack(id);
            unpacker.unpack(is_last);
        }
    }
};

struct MachineStatus{
    uint8_t msgType; 
    char state[STATE_LEN]; 
    int id; 
    bool ack; 
    
    void to_msgpack(MsgPack::Packer& packer) const 
    {
        packer.pack(arduino::msgpack::arr_size_t(4));
        packer.pack(msgType);
        packer.pack(state);
        packer.pack(id);
        packer.pack(ack);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) 
    {
        unpacker.unpack(msgType);
        static std::vector<char> temp;
        if(temp.capacity() < STATE_LEN) 
        {
            temp.reserve(STATE_LEN + 5);
        }
        unpacker.unpack(temp);
        size_t len = temp.size();
        if(len >= STATE_LEN) len = STATE_LEN - 1;

        memcpy(state, temp.data(), len);
        state[len] = '\0';
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