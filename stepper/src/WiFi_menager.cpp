#include "Wifi_menger.h"

WiFiMenager::WiFiMenager(char* ssid, char* pass):
ssid(ssid), password(pass), port(0), serverIP(IPAddress(0,0,0,0)), message_callback(nullptr) {}

void WiFiMenager::set_callback(on_message_callback ms)
{
    this->message_callback = ms;
}

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
    while (WiFi.status() != WL_CONNECTED && retry <= 60)
    {
        delay(500);
        if(Serial)
        {
           Serial.print('.');
        }  
        retry++;
    }
    if(Serial) Serial.println();
    if(WiFi.status() == WL_CONNECTED)
    {
        if(Serial)
        {
            Serial.print("[WIFI] Connected: ");
            Serial.println(WiFi.localIP());
            Serial.print("[WIFI] RSSI: ");
            Serial.println(WiFi.RSSI());
        }
        if(MDNS.begin(this->service_name))
        {
            if(Serial) Serial.println("[mDMS] the service has been activated");
        }

        client.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
            this->web_socket_events(type, payload, length);
        });

        client.setReconnectInterval(this->interval);
        return true;
    }
    else
    {
        if(Serial) {
            Serial.print("[WIFI] Connection FAILED. Status code: ");
            Serial.println(WiFi.status());
            Serial.println("[WIFI] Error codes: [1 = No network], [4 = Wrong password/Error], [6 = Disconnected]");
        }
        return false;
    }
}

bool WiFiMenager::find_server()
{
    if(this->serverIP != IPAddress(0,0,0,0)) return true;
    if(WiFi.status() !=  WL_CONNECTED) return false;

    if(Serial) Serial.println("[mDNS] looking for a server");
    int n = MDNS.queryService(this->server_name, this->server_protocol);

    if(n > 0)
    {
        serverIP = MDNS.IP(0);
        port = MDNS.port(0);
        if(Serial) {
            Serial.print("[mDNS] Server found: ");
            Serial.println(serverIP);
            Serial.print("[mDNS] Port: ");
            Serial.println(port);
        }
        return true;
    }

    return false;
}

void WiFiMenager::run()
{
    if(serverIP == IPAddress(0,0,0,0))
    {
        static unsigned long last_search = 0;
        if(millis() - last_search > 5000)
        {
            last_search = millis();
            if(find_server())
            {
                client.begin(serverIP, port, "/");
            }
        }
    }
    else
    {
        client.loop();
    }
    MDNS.update();
}

bool WiFiMenager::is_connected()
{
    return client.isConnected();
}

void WiFiMenager::web_socket_events(WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
            if(Serial) Serial.println("[WeebSocket] Disconnected");
        break;
        case WStype_CONNECTED:
            if(Serial) Serial.println("[WeebSocket] Connected");
        break;
        case WStype_TEXT:
        case WStype_BIN:
            if(message_callback != nullptr)
            {
                message_callback(payload, length, type);
            }
        break;
    }
}

bool WiFiMenager::send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer)
{
    if(!this->is_connected()) return false;
    packer.clear();
    packer.serialize(meassage);
    if(client.sendBIN(packer.data(), packer.size())) 
    {
        if(Serial) Serial.println("[WeebSocket] Message sent");
        return true;
    }
    if(Serial) Serial.println("[WeebSocket] Message NOT sent");
    return false;
}