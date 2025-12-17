
#ifndef WIFI_MENAGER_H
#define WIFI_MENAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <LEAmDNS.h>
#include "packets.h"
#include <MsgPack.h>
#include <LittleFS.h>     
#include <ArduinoJson.h>

typedef void (*on_message_callback)(uint8_t num, uint8_t* payload, size_t lenght, WStype_t type);


class WiFiMenager {
private:
    const char* ssid; 
    const char* password; 

    int port_webSocket = 81; 
    int port_http = 80;

    const char* host_name = "winder";
    WebServer server;
    on_message_callback message_callback; 
    
    WebSocketsServer websocket;
  
    void web_socket_events(uint8_t num ,WStype_t type, uint8_t* payload, size_t length);

public:

    WiFiMenager(char* ssid, char* pass);
    void set_callback(on_message_callback ms);
    bool init();
    void run();
    bool send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer);
    String get_content_type(String filename);
    bool handle_file_read(String path);
    void handle_config_get();
    void handle_config_post();
    bool load_config(float &sx, float &sy, float &st_mm, float &st_rot);
    void handle_jog();
};

#endif