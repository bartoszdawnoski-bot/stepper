#include "Wifi_menger.h"

WiFiMenager::WiFiMenager(char* ssid, char* pass):
ssid(ssid), password(pass), message_callback(nullptr), websocket(8080), server(80) {}

// Ustawienie wskaźnika na funkcję, która zostanie wywołana po odebraniu danych
void WiFiMenager::set_callback(on_message_callback ms)
{
    this->message_callback = ms;
}

String WiFiMenager::get_content_type(String filename)
{
    if (filename.endsWith(".html")) return "text/html";
    else if (filename.endsWith(".css")) return "text/css";
    else if (filename.endsWith(".js")) return "application/javascript";
    else if (filename.endsWith(".json")) return "application/json";
    else if (filename.endsWith(".ico")) return "image/x-icon";
    return "text/plain";
} 

bool WiFiMenager::handle_file_read(String path)
{
    if (Serial) Serial.print("[HTTP] Request: "); Serial.println(path);
    if (path.endsWith("/")) path += "index.html";
    if(LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, get_content_type(path));
        file.close();
        return true;
    }
    return false;
}

void WiFiMenager::handle_config_get()
{
    if(LittleFS.exists("/config.json"))
    {
        File file = LittleFS.open("/config.json", "r");
        server.streamFile(file, "application/json");
        file.close();
    }
    else
    {
        server.send(200, "application/json", "{}");
    }
}

void WiFiMenager::handle_config_post()
{
    if (server.hasArg("plain") == false) 
    {
        server.send(400, "text/plain", "Body not received");
        return;
    }

    String body = server.arg("plain");

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) 
    {
        if(Serial) Serial.println("[Config] JSON parsing error");
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    // Otwórz plik do zapisu ("w" nadpisuje plik)
    File file = LittleFS.open("/config.json", "w");
    if (!file) 
    {
        server.send(500, "text/plain", "Failed to open file for writing");
        return;
    }

    // Zapisz JSON do pliku
    if (serializeJson(doc, file) == 0) 
    {
        server.send(500, "text/plain", "Failed to write to file");
    } 
    else 
    {
        server.send(200, "text/plain", "Config Saved");
        if(Serial) Serial.println("[Config] Settings updated");
    }
    file.close();
    
}
// Inicjalizacja połączenia WiFi i usług sieciowych
bool WiFiMenager::init()
{
    if(!LittleFS.begin()) 
    {
        if(Serial) Serial.println("[FS] LittleFS Mount Failed. Formatting");
        LittleFS.format();
        if(LittleFS.begin()) 
        {
            if(Serial) Serial.println("[FS] Formatted and Mounted");
        } 
        else 
        {
            if(Serial) Serial.println("[FS] Critical Error, Unable to mount FS");
        }
    } 
    else 
    {
        if(Serial) Serial.println("[FS] Mounted successfully.");
    }

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
        //Start mDNS dostęp jako ws://winder.local:8080
        if(MDNS.begin(this->host_name))
        {
            MDNS.addService("ws", "tcp", this->port_webSocket);
            MDNS.addService("http", "tcp", this->port_http);
            if(Serial) Serial.println("[mDMS] Started: winder.local");
        }

        server.on("/api/config", HTTP_GET, [this](){ this->handle_config_get(); });
        server.on("/api/config", HTTP_POST, [this](){ this->handle_config_post(); });
        server.on("/api/jog", HTTP_GET, [this](){ this->handle_jog(); });

        server.onNotFound([this]() 
        {
            // Próbuj znaleźć plik w pamięci. 
            if (!this->handle_file_read(this->server.uri())) 
            {
                this->server.send(404, "text/plain", "404: File Not Found");
            }
        });

        server.begin();
        //Start Serwera WebSocket
        websocket.begin();
        websocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            this->web_socket_events(num, type, payload, length);
        });

        // Ustawienie interwału automatycznego ponawiania połączenia
        if(Serial) Serial.print("[WS] WebSocket Server started on port "); Serial.println(this->port_webSocket);
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

// Główna pętla obsługi sieci (powinna być wywoływana w loop())
void WiFiMenager::run()
{
    server.handleClient();
    // Obsługa klientów 
    websocket.loop();
    // Obsługa mDNS
    MDNS.update();
}

bool WiFiMenager::load_config(float &sx, float &sy, float &st_mm, float &st_rot) {
    if (!LittleFS.exists("/config.json")) return false;
    
    File file = LittleFS.open("/config.json", "r");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) 
    {
        if(Serial) Serial.println("[Config] Failed to read file, using defaults");
        return false;
    }
    // Pobierz wartości 
    if(doc.containsKey("max_speed_x")) sx = doc["max_speed_x"];
    if(doc.containsKey("max_speed_y")) sy = doc["max_speed_y"];
    if(doc.containsKey("steps_mm")) st_mm = doc["steps_mm"];
    if(doc.containsKey("steps_rot")) st_rot = doc["steps_rot"];

    return true;
}

// Wewnętrzna obsługa zdarzeń WebSockets
void WiFiMenager::web_socket_events(uint8_t num ,WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
            if(Serial) Serial.printf("[%u] Disconnected!\n", num);
        break;
        case WStype_CONNECTED:
        {
            IPAddress ip = websocket.remoteIP(num);
            if(Serial) Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
        }
        break;
        case WStype_TEXT:
        {
            if(Serial) Serial.printf("[%u] TXT: %s\n", num, payload);
            if(message_callback != nullptr) message_callback(num, payload, length, type);
        }
        case WStype_BIN:
            if(message_callback != nullptr) message_callback(num, payload, length, type);
        break;
    }
}

// Funkcja wysyłająca strukturę MachineStatus jako MsgPack
bool WiFiMenager::send_msgpack(MachineStatus meassage,  MsgPack::Packer& packer)
{
    packer.clear();
    packer.serialize(meassage);
    return websocket.broadcastBIN(packer.data(), packer.size());
}

void WiFiMenager::handle_jog() {
    if (server.hasArg("cmd")) 
    {
        String cmd = server.arg("cmd");
        if (cmd == "HOME") 
        {
            if(message_callback != nullptr) message_callback(255, (uint8_t*)"G28", 3, WStype_TEXT);
            server.send(200, "text/plain", "OK");
            return;
        }
    }

    if (!server.hasArg("axis") || !server.hasArg("dist")) 
    {
        server.send(400, "text/plain", "Missing args");
        return;
    }
    String axis = server.arg("axis"); // "X" lub "Y"
    String dist = server.arg("dist"); // np. "10" lub "-10"
    String speed = server.arg("speed"); // np. "500"
    if(speed == "") speed = "500"; // Domyślna prędkość

    String gcode = "G1 " + axis + dist + " F" + speed;

    if(message_callback != nullptr) 
    {
        message_callback(255, (uint8_t*)gcode.c_str(), gcode.length(), WStype_TEXT);
    }

    server.send(200, "text/plain", "OK");
}