#ifndef PACKETS_H
#define PACKETS_H

#include <Arduino.h>
#include <ArduinoJson.h> // Zmiana z MsgPack na ArduinoJson

// Rozmiary buforów
#define MAX_GCODE_LEN 64 
#define STATE_LEN 20

struct GcodeCom{
    uint8_t msgType; 
    char Gcode[MAX_GCODE_LEN]; 
    int id;
    bool is_last; 
    
    // Serializacja do JSON
    void to_json(JsonObject& doc) const 
    {
        doc["t"] = msgType;  
        doc["g"] = Gcode;   
        doc["i"] = id;       
        doc["l"] = is_last;      
    }

    // Deserializacja z JSON
    void from_json(const JsonObject& doc) 
    {
        msgType = doc["msgType"] | 0;
        
        const char* gcodePtr = doc["Gcode"];
        if (gcodePtr) 
        {
            strncpy(Gcode, gcodePtr, MAX_GCODE_LEN);
            Gcode[MAX_GCODE_LEN - 1] = '\0';
        } 
        else 
        {
            Gcode[0] = '\0';
        }

        id = doc["id"] | 0;
        is_last = doc["is_last"] | false;
    }
};

struct MachineStatus{
    uint8_t msgType; 
    char state[STATE_LEN]; 
    int id; 
    bool ack; 
    
    void to_json(JsonObject& doc) const 
    {
        doc["t"] = msgType;  
        doc["s"] = state;    
        doc["i"] = id;       
        doc["a"] = ack;      
    }

    void from_json(const JsonObject& doc) 
    {
        msgType = doc["msgType"] | 0;

        const char* statePtr = doc["state"];
        if (statePtr)
        {
            strncpy(state, statePtr, STATE_LEN);
            state[STATE_LEN - 1] = '\0';
        } 
        else 
        {
            state[0] = '\0';
        }

        id = doc["id"] | 0;
        ack = doc["ack"] | false;
    }
};

// Struktury wewnętrzne (bez zmian w logice serializacji, bo żyją tylko w RAM)
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