#include "Wifi_menger.h"

WiFiMenager::WiFiMenager(char* ssid, char* pass):
ssid(ssid), password(pass), port(0), serverIP(IPAddress(0,0,0,0)), message_callback(nullptr) {}

// Ustawienie wskaźnika na funkcję, która zostanie wywołana po odebraniu danych
void WiFiMenager::set_callback(on_message_callback ms)
{
    this->message_callback = ms;
}

// Inicjalizacja połączenia WiFi i usług sieciowych
bool WiFiMenager::init()
{
    // Jeśli już połączono, nie rób nic
    if(WiFi.status() == WL_CONNECTED) return true;
    if(Serial)
    {
        Serial.print("[WIFI] connect to: ");
        Serial.println(this->ssid);
    }

    WiFi.mode(WIFI_STA); // Tryb stacji (klienta)
    WiFi.begin(ssid, password);

    // Próba połączenia z timeoutem (ok. 30 sekund)
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
            Serial.print("[WIFI] RSSI: "); // Siła sygnału
            Serial.println(WiFi.RSSI());
        }
        // Uruchomienie mDNS urządzenie ogłasza się jako "winder.local"
        if(MDNS.begin(this->service_name))
        {
            if(Serial) Serial.println("[mDMS] the service has been activated");
        }

        // Konfiguracja obsługi zdarzeń WebSockets (lambda przekazująca do metody klasy)
        client.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
            this->web_socket_events(type, payload, length);
        });

        // Ustawienie interwału automatycznego ponawiania połączenia
        client.setReconnectInterval(this->interval);
        return true;
    }
    else
    {
        // Logowanie błędów połączenia
        if(Serial) {
            Serial.print("[WIFI] Connection FAILED. Status code: ");
            Serial.println(WiFi.status());
            Serial.println("[WIFI] Error codes: [1 = No network], [4 = Wrong password/Error], [6 = Disconnected]");
        }
        return false;
    }
}

// Wyszukiwanie serwera w sieci lokalnej za pomocą mDNS
bool WiFiMenager::find_server()
{
    // Jeśli już mamy IP serwera lub brak WiFi, przerwij
    if(this->serverIP != IPAddress(0,0,0,0)) return true;
    if(WiFi.status() !=  WL_CONNECTED) return false;

    if(Serial) Serial.println("[mDNS] looking for a server");
    // Zapytanie mDNS o usługę "winding_server" na protokole TCP
    int n = MDNS.queryService(this->server_name, this->server_protocol);

    if(n > 0)
    {
        // Znaleziono serwer pobierz IP i port
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

// Główna pętla obsługi sieci (powinna być wywoływana w loop())
void WiFiMenager::run()
{
    // Jeśli nie mamy IP serwera, próbuj go znaleźć co 5 sekund
    if(serverIP == IPAddress(0,0,0,0))
    {
        static unsigned long last_search = 0;
        if(millis() - last_search > 5000)
        {
            last_search = millis();
            if(find_server())
            {
                // Po znalezieniu serwera, nawiąż połączenie WebSocket
                client.begin(serverIP, port, "/");
            }
        }
    }
    else
    {
        // Jeśli mamy IP, obsługuj pętlę klienta WebSocket
        client.loop();
    }
    // Obsługa zapytań mDNS
    MDNS.update();
}

bool WiFiMenager::is_connected()
{
    return client.isConnected();
}

// Wewnętrzna obsługa zdarzeń WebSockets
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
        // Jeśli przyszły dane (tekst lub binarne), przekaż do callbacka w main.cpp
            if(message_callback != nullptr)
            {
                message_callback(payload, length, type);
            }
        break;
    }
}

// Funkcja wysyłająca strukturę MachineStatus jako MsgPack
bool WiFiMenager::send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer)
{
    if(!this->is_connected()) return false;
    packer.clear(); // Wyczyść bufor packera
    packer.serialize(meassage); // Serializuj strukturę do formatu binarnego
    // Wyślij dane binarne przez WebSocket
    if(client.sendBIN(packer.data(), packer.size())) 
    {
        if(Serial) Serial.println("[WeebSocket] Message sent");
        return true;
    }
    if(Serial) Serial.println("[WeebSocket] Message NOT sent");
    return false;
}