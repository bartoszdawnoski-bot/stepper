#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <MsgPack.h>

// Struktura odbierana od PC (Komenda G-Code)
struct GcodeCom{
    uint8_t msgType; //typ wiadomosci  
    String Gcode; //tresc komendy
    int id;//id komendy
    bool is_last; // Flaga oznaczająca koniec przesyłania pliku
    // Makro biblioteki MsgPack definiujące, które pola mają być serializowane
    MSGPACK_DEFINE(msgType, Gcode, id, is_last);
};

// Struktura wysyłana do PC (Status maszyny/Potwierdzenie)
struct MachineStatus{
    uint8_t msgType; //typ wiadomosci  
    String state; //status nawijarki
    int id; //id wykonanej komendy
    bool ack; //ack 
    // Makro biblioteki MsgPack
    MSGPACK_DEFINE(msgType, state, id, ack);
};

struct processedStatus{
    uint8_t msgType; //typ wiadomosci  
    String state; //status nawijarki
    int id; //id wykonanej komendy
    bool ack; //ack 
    uint8_t target_client;
    // Makro biblioteki MsgPack
    MSGPACK_DEFINE(msgType, state, id, ack);
};

struct processedData{
    uint8_t msgType; //typ wiadomosci  
    String Gcode; //tresc komendy
    int id;//id komendy
    bool is_last; // Flaga oznaczająca koniec przesyłania pliku
    uint8_t client_num;
};

#endif
