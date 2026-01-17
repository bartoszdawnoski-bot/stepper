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
    MSGPACK_DEFINE(msgType, Gcode, id, is_last);
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
    MSGPACK_DEFINE(msgType, state, x, y, z, id, ack);
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
};

#endif
