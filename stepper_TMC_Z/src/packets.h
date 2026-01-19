#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <MsgPack.h>

// Struktura odbierana od PC (Komenda G-Code)
struct GcodeCom{
    uint8_t msgType; //typ wiadomosci  
    char Gcode[64]; //tresc komendy
    int id;//id komendy
    bool is_last; // Flaga oznaczająca koniec przesyłania pliku
    // Makro biblioteki MsgPack definiujące, które pola mają być serializowane
    void to_msgpack(MsgPack::Packer& packer) const {
        packer.pack(arduino::msgpack::arr_size_t(4));
        packer.pack(msgType);
        packer.pack(Gcode);
        packer.pack(id);
        packer.pack(is_last);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) {
        arduino::msgpack::arr_size_t sz;
        if (unpacker.unpack(sz)) {
            unpacker.unpack(msgType);
            
            // Odbieramy String tymczasowo i przepisujemy do char[]
            String temp;
            unpacker.unpack(temp);
            strncpy(Gcode, temp.c_str(), sizeof(Gcode));
            Gcode[sizeof(Gcode)-1] = 0; // Bezpiecznik

            unpacker.unpack(id);
            unpacker.unpack(is_last);
        }
    }
};

// Struktura wysyłana do PC (Status maszyny/Potwierdzenie)
struct MachineStatus{
    uint8_t msgType; //typ wiadomosci  
    char state[10]; //status nawijarki
    float x;
    float y;
    float z;
    int id; //id wykonanej komendy
    bool ack; //ack 
    // Makro biblioteki MsgPack
    void to_msgpack(MsgPack::Packer& packer) const {
        packer.pack(arduino::msgpack::arr_size_t(7));
        packer.pack(msgType);
        packer.pack(state);
        packer.pack(x);
        packer.pack(y);
        packer.pack(z);
        packer.pack(id);
        packer.pack(ack);
    }
    
    void from_msgpack(MsgPack::Unpacker& unpacker) {
        // Ignorujemy (nie odbieramy statusu)
    }
};

struct processedStatus{
    uint8_t msgType; //typ wiadomosci  
    char state[10]; //status nawijarki
    float x;
    float y;
    float z;
    int id; //id wykonanej komendy
    bool ack; //ack 
    uint8_t target_client;
};

struct processedData{
    uint8_t msgType; //typ wiadomosci  
    char Gcode[64]; //tresc komendy
    int id;//id komendy
    bool is_last; // Flaga oznaczająca koniec przesyłania pliku
    uint8_t client_num;

    void to_msgpack(MsgPack::Packer& packer) const {
        packer.pack(arduino::msgpack::arr_size_t(4)); 
        packer.pack(msgType);
        packer.pack(Gcode);
        packer.pack(id);
        packer.pack(is_last);
    }

    void from_msgpack(MsgPack::Unpacker& unpacker) {
        arduino::msgpack::arr_size_t sz;
        if (unpacker.unpack(sz)) { 
            unpacker.unpack(msgType);

            // FIX: Odbierz jako String -> przepisz do char[]
            String temp;
            unpacker.unpack(temp);
            strncpy(Gcode, temp.c_str(), sizeof(Gcode));
            Gcode[sizeof(Gcode)-1] = 0; // Bezpiecznik

            unpacker.unpack(id);
            unpacker.unpack(is_last);
        }
    }
};

#endif
