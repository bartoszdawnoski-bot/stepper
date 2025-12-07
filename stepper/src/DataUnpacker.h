#define DATAUNPACKER_H
#ifdef DATAUNPACKER_H

#include <MsgPack.h>
#include <Arduino.h>

class DataUnpacker {
private:
    MsgPack::Unpacker unpacker;
    struct GCodeChunkData {
        MsgPack::bin_t binData; // Reprezentuje dane binarne (chunk G-Code)
        bool finished;          // Flaga bool (czy to ostatni pakiet)
        MSGPACK_DEFINE(binData, finished); // Makro do automatycznej serializacji/deserializacji
    };
public:
    struct decoded_packet
    {
        const uint8_t* data;
        size_t size;
        bool is_finish;
    };

    void feed(uint8_t* buffer, size_t size);
    bool unpack(decoded_packet& result);
    void clear();
};


#endif