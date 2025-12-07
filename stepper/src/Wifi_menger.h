#define WIFI_MENAGER_H
#ifdef WIFI_MENAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

class WiFiMenager {
private:
    const char* ssid;
    const char* password;
    int port;
    int udp_port;

    IPAddress serverIP;
    WiFiUDP udp;
    WiFiClient client;

    const char* discovery_msg = "DISCOVER_WINDER";
    const char* server_response = "I_AM_SERVER";
public:
    WiFiMenager(char* ssid, char* pass, int port, int udp);
    bool init();
    bool find_server();
    bool connect_to_pc();
    bool has_data();
    int read_packet(uint8_t* buffer, size_t size);
    void send_status(char* state, float progress);
    void send_ACK();
    void stop();
};

#endif