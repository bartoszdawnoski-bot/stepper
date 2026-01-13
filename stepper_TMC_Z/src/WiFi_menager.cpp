#include "Wifi_menger.h"

const char* BLOCKED_CONFIG_HTML = 
    R"rawliteral(
        <!DOCTYPE html>
        <html>
            <head>
                <meta charset="utf-8">
                <meta name="viewport" content="width=device-width, initial-scale=1">
                <title>Dostęp zabroniony</title>
                <style>
                    body { font-family: sans-serif; text-align: center; padding: 20px; background-color: #ffe6e6; color: #cc0000; }
                    .container { max-width: 600px; margin: 50px auto; border: 2px solid #cc0000; padding: 20px; border-radius: 10px; background: white; }
                    h1 { font-size: 24px; }
                    a { display: inline-block; margin-top: 20px; padding: 10px 20px; background: #333; color: white; text-decoration: none; border-radius: 5px; }
                </style>
            </head>
            <body>
                <div class="container">
                    <h1>Konfiguracja zablokowana</h1>
                    <p>Maszyna jest obecnie połączona z panelem sterowania (WebSocket).</p>
                    <p>Ze względów bezpieczeństwa zmiana ustawień jest niemożliwa podczas pracy.</p>
                    <p>Rozłącz panel sterowania, aby odblokować konfigurację.</p>
                </div>
            </body>
        </html>
    )rawliteral";

// Konstruktor przepisuje char* do String (automatycznie)
WiFiMenager::WiFiMenager(char* ssid, char* pass):
ssid(ssid), password(pass), message_callback(nullptr), websocket(8080), server(80) {}

WiFiMenager::WiFiMenager():
ssid(""), password(""), message_callback(nullptr), websocket(8080), server(80) {}

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

    File file = LittleFS.open("/config.json", "w");
    if (!file) 
    {
        server.send(500, "text/plain", "Failed to open file for writing");
        return;
    }

    if (serializeJson(doc, file) == 0) 
    {
        server.send(500, "text/plain", "Failed to write to file");
    } 
    else 
    {
        server.send(200, "text/plain", "Config Saved");
        if(Serial) Serial.println("[Config] Settings updated");

        this->config_changed = true;
    }
    file.close();   
}

bool WiFiMenager::serial_wizard()
{
    Serial.setTimeout(30000);
    Serial.println("[WIZARD] WIFI Connection wizard");
    Serial.println("[WIZARD] Network scanning");

    WiFi.disconnect();

    int n = WiFi.scanNetworks();
    if(n == 0)
    {
        Serial.println("[WIZARD] No networks found");
        return false;
    }

    Serial.println();
    Serial.println("[WIZARD] Available networks:");
    for (int i = 0; i < n; ++i)
    {
        Serial.print("["); Serial.print(i + 1); Serial.print("] ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (Signal: "); Serial.print(WiFi.RSSI(i)); Serial.print("dBm)");
        Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " [Open]" : " [Secured]");
        delay(10);
    }
    Serial.println();
    Serial.println("[WIZARD] Enter network number:");
    Serial.flush();

    while(Serial.available() == 0) { delay(100); }
    int choice = Serial.parseInt();
    while(Serial.available()) { Serial.read(); }

    if(choice < 1 || choice > n)
    {
        Serial.println("[WIZARD] Wrong number");
        return serial_wizard();
    }

    String select_ssid = WiFi.SSID(choice - 1);
    String select_pass = "";
    Serial.print("[WIZARD] Selected: "); Serial.println(select_ssid);
    if(WiFi.encryptionType(choice - 1) != ENC_TYPE_NONE)
    {
        Serial.println("[WIZARD] Enter password (and press Enter): ");
        while(Serial.available() == 0) { delay(50); }
        select_pass = Serial.readStringUntil('\n');
        select_pass.trim(); 
    }

    StaticJsonDocument<1024> doc;

    if(LittleFS.exists("/config.json"))
    {
        File file = LittleFS.open("/config.json", "r");
        deserializeJson(doc, file);
        file.close();
    }

    doc["ssid"] = select_ssid;
    doc["pass"] = select_pass;

    File file = LittleFS.open("/config.json", "w");
    if(serializeJson(doc, file))
    {
        Serial.println("[WIZARD] Saved successfully"); 
        file.close();
        delay(1000);
        rp2040.reboot();
    }
    else
    {
        Serial.println("[WIZARD] Failed to write file");
        file.close();
        return false;
    }    
    return true;
}

bool WiFiMenager::init()
{
    if(!LittleFS.begin()) 
    {
        if(Serial) Serial.println("[FS] LittleFS Mount Failed. Formatting");
        LittleFS.format();
        if(LittleFS.begin()) Serial.println("[FS] Formatted and Mounted");
        else Serial.println("[FS] Critical Error, Unable to mount FS");
    } 
    else 
    {
        if(Serial) Serial.println("[FS] Mounted successfully.");
    }
   
    if(WiFi.status() == WL_CONNECTED) return true; 

    if(Serial)
    {
        delay(2000); 

        Serial.println("[WIFI] Press the key to configure WiFi");

        unsigned long startWait = millis();
        bool enterSetup = false;
        
        Serial.flush();
        while(millis() - startWait < 3000)
        {
            if (millis() % 500 == 0) { Serial.print("."); delay(20); }
            if (Serial.available()) 
            {
                enterSetup = true;
                while(Serial.available()) Serial.read(); 
                break;
            }
        }
        Serial.println();
        
        if (enterSetup) 
        {
            serial_wizard(); 
            return false;
        } else {
            Serial.println("[WIFI] Start from saved data");
        }
    }

    String name = "";
    String pass = "";

    if(wifi_get(name, pass) && this->ssid == "" && this->password == "")
    {
        this->ssid = name;
        this->password = pass;
    }

    if(name == "")
    {
        if(Serial) Serial.println("[WIFI] No configuration in the file");
        serial_wizard();
        return false;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(this->ssid.c_str(), this->password.c_str());

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry <= 30)
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
            Serial.print("[WIFI] RSSI: "); 
            Serial.println(WiFi.RSSI());
        }
        
        if(MDNS.begin(this->host_name))
        {
            MDNS.addService("ws", "tcp", this->port_webSocket);
            MDNS.addService("http", "tcp", this->port_http);
            if(Serial) Serial.println("[mDMS] Started: winder.local");
        }

        server.on("/api/config", HTTP_GET, [this](){ this->handle_config_get(); });
        server.on("/api/config", HTTP_POST, [this](){ this->handle_config_post(); });
        server.on("/api/jog", HTTP_GET, [this](){ this->handle_jog(); });
        server.on("/index.html", HTTP_GET, [this](){
            if(this->active_clients > 0) this->server.send(200, "text/html", BLOCKED_CONFIG_HTML);
            else 
            {
                if(!this->handle_file_read("/index.html")) this->server.send(404, "text/plain", "Config file not found");
            }
        });
        server.on("/", HTTP_GET, [this](){
            if(this->active_clients > 0) this->server.send(200, "text/html", BLOCKED_CONFIG_HTML);
            else 
            {
                if(!this->handle_file_read("/index.html")) this->server.send(404, "text/plain", "Config file not found");
            }
        });
        server.onNotFound([this]() 
        {
            if (!this->handle_file_read(this->server.uri())) 
            {
                this->server.send(404, "text/plain", "404: File Not Found");
            }
        });

        server.begin();
        websocket.begin();
        websocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            this->web_socket_events(num, type, payload, length);
        });

        if(Serial) { Serial.print("[WS] WebSocket Server started on port "); Serial.println(this->port_webSocket); }
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

void WiFiMenager::run()
{
    server.handleClient();
    websocket.loop();
    MDNS.update();
}

bool WiFiMenager::load_config(float &sx, float &sy, float &sz, float &st_mm, float &st_rot, float &st_br) {
    if(!LittleFS.exists("/config.json")) return false;
    
    File file = LittleFS.open("/config.json", "r");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if(error) 
    {
        if(Serial) Serial.println("[Config] Failed to read file");
        return false;
    }
    if(doc.containsKey("max_speed_x")) sx = doc["max_speed_x"];
    if(doc.containsKey("max_speed_y")) sy = doc["max_speed_y"];
    if(doc.containsKey("max_speed_z")) sz = doc["max_speed_z"];
    if(doc.containsKey("steps_mm")) st_mm = doc["steps_mm"];
    if(doc.containsKey("steps_rot")) st_rot = doc["steps_rot"];
    if(doc.containsKey("steps_br")) st_br = doc["steps_br"];

    return true;
}

bool WiFiMenager::wifi_get(String &ssid, String &pass)
{
    if(!LittleFS.exists("/config.json")) return false;
    
    File file = LittleFS.open("/config.json", "r");
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) 
    {
        if(Serial) Serial.println("[Config] Failed to read file");
        return false;
    }

    if(doc.containsKey("ssid")) ssid = doc["ssid"].as<String>();
    if(doc.containsKey("pass")) pass = doc["pass"].as<String>();
    
    return true;
}

void WiFiMenager::web_socket_events(uint8_t num ,WStype_t type, uint8_t* payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
            if(this->active_clients > 0) active_clients--;
            if(Serial) 
            {
                Serial.printf("[%u] Disconnected!", num);
                Serial.println();
            }
        break;
        case WStype_CONNECTED:
        {
            active_clients++;
            IPAddress ip = websocket.remoteIP(num);
            if(Serial) 
            {
                Serial.printf("[%u] Connected from %d.%d.%d.%d", num, ip[0], ip[1], ip[2], ip[3]);
                Serial.println();
            }
        }
        break;
        case WStype_TEXT:
        {
            if(Serial) 
            {
                Serial.printf("[%u] TXT: %s\n", num, payload);
                Serial.println();
            }
            if(message_callback != nullptr) message_callback(num, payload, length, type);
        }
        break;
        case WStype_BIN:
            if(message_callback != nullptr) message_callback(num, payload, length, type);
        break;
    }
}

bool WiFiMenager::send_msgpack(uint8_t client_num, MachineStatus &data, MsgPack::Packer &packer)
{
    packer.clear();
    packer.serialize(data);
    return websocket.sendBIN(client_num, packer.data(), packer.size());
}

void WiFiMenager::handle_jog() 
{
    if (server.hasArg("cmd")) 
    {
        String cmd = server.arg("cmd");
        if (cmd == "HOME") 
        {
            if(message_callback != nullptr) message_callback(255, (uint8_t*)"G28", 3, WStype_TEXT);
            server.send(200, "text/plain", "OK");
            return;
        }
        else
        {
            if(message_callback != nullptr) message_callback(255, (uint8_t*)cmd.c_str(), cmd.length(), WStype_TEXT);
            server.send(200, "text/plain", "OK");
            return;
        }
    }

    if (!server.hasArg("axis") || !server.hasArg("dist")) 
    {
        server.send(400, "text/plain", "Missing args");
        return;
    }
    String axis = server.arg("axis"); 
    String dist = server.arg("dist"); 
    String speed = server.arg("speed"); 
    if(speed == "") speed = "500"; 

    String gcode = "G1 " + axis + dist + " F" + speed;

    if(message_callback != nullptr) 
    {
        message_callback(255, (uint8_t*)gcode.c_str(), gcode.length(), WStype_TEXT);
    }

    server.send(200, "text/plain", "OK");
}

bool WiFiMenager::isCon()
{   
    return WiFi.connected();
}

void WiFiMenager::send_stop()
{ 
    MachineStatus stop;
    stop.msgType = 0xFFF;
    stop.id = -1;
    stop.ack = false;
    stop.state = "EStop";

    MsgPack::Packer packer;
    packer.serialize(stop);
    websocket.broadcastBIN(packer.data(), packer.size());

}