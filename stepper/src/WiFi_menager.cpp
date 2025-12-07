#include "Wifi_menger.h"

WiFiMenager::WiFiMenager(char* ssid, char* pass, int port, int udp):
ssid(ssid), password(pass), port(port), udp_port(udp), serverIP(IPAddress(0,0,0,0)) {}

bool WiFiMenager::init()
{
    if(WiFi.status() == WL_CONNECTED) return true;
    if(Serial)
    {
        Serial.print("[WIFI] connect to: ");
        Serial.println(this->ssid);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry <= 20)
    {
        delay(500);
        if(Serial) Serial.print('.'); 
        retry++;
    }
    if(Serial) Serial.println();
    
    if(WiFi.status() == WL_CONNECTED)
    {
        if(Serial)
        {
            Serial.print("[WIFI] Connected: ");
            Serial.println(WiFi.localIP());
        }
        if(udp.begin(udp_port)) return true;
        else
        {
           if(Serial)Serial.print("[WIFI] UDP not init");
           return false;
        }
    }
    else
    {
        if(Serial)Serial.print("[WIFI] Connection error with the router");
        return false;
    }
}

bool WiFiMenager::find_server()
{
    if(this->client.connected()) return true;

    udp.beginPacket(IPAddress(255, 255, 255, 255), this->udp_port);
    udp.write((uint8_t*) this->discovery_msg, 15);
    udp.endPacket();

    unsigned long start = millis();
    while(millis() - start <= 200)
    {
        int size = udp.parsePacket();
        if(size) 
        {
            char packet[255];
            int len = udp.read(packet, 255);
            if(len > 0) packet[len] = 0;

            if(strstr(packet, this->server_response))
            {
                this->serverIP = udp.remoteIP();
                if(Serial) 
                {
                    Serial.print("[UDP] Server found: ");
                    Serial.println(serverIP);
                }
                return true;
            }
        }
    }
    return false;
}

bool WiFiMenager::connect_to_pc()
{
    if(this->client.connected()) return true;

    if(serverIP == IPAddress(0,0,0,0))
    {
        if (!this->find_server()) return false;
    }

    if(Serial) Serial.print("[WIFI] TCP connecting... ");
    if(client.connect(serverIP, port))
    {
        if(Serial) Serial.println("OK");
        return true;
    }
    return false;
}

bool WiFiMenager::has_data() 
{
    return this->client.connected() && this->client.available();
}

int WiFiMenager::read_packet(uint8_t* buffer, size_t size)
{
    if(has_data())
    {
        return this->client.read(buffer, size);
    }
    return 0;
}

void WiFiMenager::send_status(char* state, float progress)
{
    if(this->client.connected())
    {
        client.print(state);
        client.print(" : ");
        client.println(progress, 1);
    }
}

void WiFiMenager::send_ACK()
{
    if(this->client.connected()) client.print("ACK");
}

void WiFiMenager::stop()
{
    this->client.stop();
}
