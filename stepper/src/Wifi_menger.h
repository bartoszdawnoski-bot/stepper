#define WIFI_MENAGER_H
#ifdef WIFI_MENAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <LEAmDNS.h>
#include "packets.h"
#include <MsgPack.h>

typedef void (*on_message_callback)(uint8_t* payload, size_t lenght, WStype_t type);

class WiFiMenager {
private:
    const char* ssid;
    const char* password;

    int port;
    IPAddress serverIP;
    WebSocketsClient client;

    const char* server_name = "winding_server";
    const char* service_name = "winder";
    const char* server_protocol = "tcp";

    on_message_callback message_callback;

    unsigned long interval = 5000;
    
    void web_socket_events(WStype_t type, uint8_t* payload, size_t length);

public:
    WiFiMenager(char* ssid, char* pass);
    void set_callback(on_message_callback ms);
    bool init();
    bool find_server();
    void run();
    bool is_connected();
    bool send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer);
};

#endif