#include "Wifi_menger.h"

const char* AP_CONFIG_HTML = 
R"rawliteral( 
    <!DOCTYPE html>
    <html>
        <head>
            <meta name="viewport" content="width=device-width, initial-scale=1">
            <title>WiFi Setup</title>
            <style>
                body{font-family:sans-serif;text-align:center;margin:50px;background:#f4f4f4;}
                input{margin:10px;padding:10px;font-size:16px;width:90%;max-width:300px;border:1px solid #ccc;border-radius:5px;}
                button{padding:12px 20px;font-size:16px;cursor:pointer;background:#007bff;color:white;border:none;border-radius:5px;width:90%;max-width:320px;}
            </style>
        </head>
        <body>
            <h2>Konfiguracja WiFi</h2>
            <form action="save" method="POST">
            <input type="text" name="ssid" placeholder="Nazwa sieci (SSID)" required><br>
            <input type="password" name="pass" placeholder="Haslo (zostaw puste jesli brak)"><br>
            <button type="submit">Zapisz i Restartuj</button>
            </form>
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

void WiFiMenager::ap_wizard()
{
    if(Serial) Serial.println("[WIZARD] Starting Access Point mode");

    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    WiFi.softAP("Winder_Setup");

    if(Serial)
    {
        Serial.println("[AP WIZARD] Connect to the network: Winder_Setup");
        Serial.print("[AP WIZARD] Go to the address in your browser: ");
        Serial.println(WiFi.softAPIP());
    }
    if(MDNS.begin(this->host_name))
    {
        MDNS.addService("http", "tcp", this->port_http);
    }
    server.on("/", HTTP_GET, [this]() {
        server.send(200, "text/html", AP_CONFIG_HTML);
    });

    server.on("/api/wifi", HTTP_POST, [this]() {
        String new_ssid = server.arg("ssid");
        String new_pass = server.arg("pass");

        if(Serial)  Serial.println("[AP WIZARD] New SSID: " + new_ssid);

        StaticJsonDocument<1024> doc;
        if(LittleFS.exists("/config.json")) 
        {
            File file = LittleFS.open("/config.json", "r");
            deserializeJson(doc, file);
            file.close();
        }

        doc["ssid"] = new_ssid;
        doc["pass"] = new_pass;

        File file = LittleFS.open("/config.json", "w");
        if(serializeJson(doc, file)) 
        {
            server.send(200, "text/html", "<h2>Zapisano pomyslnie!</h2><p>Pico teraz sie zrestartuje i sprobuje polaczyc z domowa siecia.</p>");
            file.close();
            delay(1500);
            rp2040.reboot(); 
        } else {
            server.send(500, "text/html", "<h2>Blad zapisu pliku config.json!</h2>");
            file.close();
        }
    });

    server.begin();
    while(true) 
    {
        server.handleClient();
        MDNS.update();
        delay(10);
    }

}

bool WiFiMenager::init()
{
    if(WiFi.status() == WL_CONNECTED) return true; 
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
   
    String saved_ssid = "";
    String saved_pass = "";
    wifi_get(saved_ssid, saved_pass);
    
    if(saved_ssid != "") 
    {
        this->ssid = saved_ssid;
        this->password = saved_pass;
    }

    if(this->ssid == "")
    {
        if(Serial) Serial.println("[WIFI] No saved configuration.");
        ap_wizard();
        return false; 
    }

    if(Serial)
    {
        Serial.println("[WIFI] Saved network found: " + this->ssid);
        Serial.println("[WIFI] Press 'c' and Enter within 2 seconds to change the configuration");

        unsigned long startWait = millis();
        bool enterSetup = false;
        
        while(millis() - startWait < 2000)
        {
            if (Serial.available()) 
            {
                char c = Serial.read();
                if (c == 'c' || c == 'C') 
                {
                    enterSetup = true;
                    break;
                }
            }
        }

        if (enterSetup) 
        {
            // Oczyszczenie bufora
            while(Serial.available()) Serial.read(); 
            ap_wizard(); 
            return false;
        }
    }
    
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.noLowPowerMode();
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
            if(!this->handle_file_read("/index.html")) this->server.send(404, "text/plain", "Config file not found");
        });
        server.on("/", HTTP_GET, [this](){
            if(!this->handle_file_read("/index.html")) this->server.send(404, "text/plain", "Config file not found");
        });
        server.onNotFound([this]() 
        {
            if (!this->handle_file_read(this->server.uri())) 
            {
                this->server.send(404, "text/plain", "404: File Not Found");
            }
        });
        server.on("/api/info", HTTP_GET, [this](){ this->handle_info_get(); });
        // Obsługa restartu Pico
        server.on("/api/reboot", HTTP_POST, [this]() {
            server.send(200, "text/plain", "Rebooting");
            delay(500);
            rp2040.reboot();
        });

        server.on("/api/macro", HTTP_POST, [this]() {
            if (!server.hasArg("plain")) {
                server.send(400, "text/plain", "Brak danych");
                return;
            }
            
            StaticJsonDocument<512> new_macros;
            deserializeJson(new_macros, server.arg("plain"));
            
            // Pobieramy stary konfig maszyny
            StaticJsonDocument<1024> full_config;
            if (LittleFS.exists("/config.json")) {
                File file = LittleFS.open("/config.json", "r");
                deserializeJson(full_config, file);
                file.close();
            }

            // Dopisujemy dane makr
            full_config["m1_name"] = new_macros["m1_name"];
            full_config["m1_cmd"] = new_macros["m1_cmd"];
            full_config["m2_name"] = new_macros["m2_name"];
            full_config["m2_cmd"] = new_macros["m2_cmd"];
            full_config["m3_name"] = new_macros["m3_name"];
            full_config["m3_cmd"] = new_macros["m3_cmd"];

            // Zapisujemy nadpisany JSON
            File file = LittleFS.open("/config.json", "w");
            if (serializeJson(full_config, file) == 0) {
                server.send(500, "text/plain", "Blad zapisu pliku config.json");
            } else {
                server.send(200, "text/plain", "Makra Zapisane");
                if(Serial) Serial.println("[Config] Makra zaktualizowane w pamieci FLASH");
            }
            file.close();
        });

        // Obsługa zmiany WiFi ze strony
        server.on("/api/wifi", HTTP_POST, [this]() {
            if (!server.hasArg("plain")) {
                server.send(400, "text/plain", "Brak danych");
                return;
            }
            
            StaticJsonDocument<512> new_wifi;
            deserializeJson(new_wifi, server.arg("plain"));
            
            StaticJsonDocument<1024> full_config;
            if (LittleFS.exists("/config.json")) {
                File file = LittleFS.open("/config.json", "r");
                deserializeJson(full_config, file);
                file.close();
            }

            // Podmieniamy tylko dane WiFi
            full_config["ssid"] = new_wifi["ssid"];
            full_config["pass"] = new_wifi["pass"];

            // Zapisujemy i robimy twardy restart
            File file = LittleFS.open("/config.json", "w");
            serializeJson(full_config, file);
            file.close();

            server.send(200, "text/plain", "Zapisano. Restart...");
            delay(1000);
            rp2040.reboot();
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
        if(Serial) 
        {
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

    static unsigned long last_mdns_update = 0;
    if (millis() - last_mdns_update > 1000) 
    {
        MDNS.update();
        last_mdns_update = millis();
    }
}

bool WiFiMenager::load_config(float &sx, float &sy, float &maxz, float &st_mm, float &st_rot) {
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
    if(doc.containsKey("max_z")) maxz = doc["max_z"];
    if(doc.containsKey("steps_mm")) st_mm = doc["steps_mm"];
    if(doc.containsKey("steps_rot")) st_rot = doc["steps_rot"];

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
                Serial.printf("[%u] Disconnected", num);
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
            if(message_callback != nullptr) message_callback(num, payload, length, type);
        }
        break;
        case WStype_BIN:
            if(Serial) Serial.println("[WS] Binary data received but not supported in JSON mode");
        break;
    }
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
    // Budowanie JSON ręcznie lub przez dokument
    StaticJsonDocument<128> doc;
    doc["msgType"] = 0xF;
    doc["id"] = -1;
    doc["ack"] = false;
    doc["state"] = "OK";

    char buffer[128];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    websocket.broadcastTXT((uint8_t*)buffer, len);
}

void WiFiMenager::reconnect()
{
    // Jeśli mamy zapisane dane logowania, próbujemy się połączyć
    if(this->ssid != "" && this->password != "") 
    {
        int retry = 0;

        if(Serial) Serial.println("[WiFi] Reconnecting...");
        if(Serial) Serial.println();

        WiFi.disconnect(); 
        delay(100);

        WiFi.begin(this->ssid.c_str(), this->password.c_str());

        while (WiFi.status() != WL_CONNECTED && retry <= 10)
        {
            delay(500);
            if(Serial) Serial.print('.');
            retry++;
        }

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
            return;
        }
        else
        {
            if(Serial)
            {
                Serial.print("[WIFI] Connection FAILED. Status code: ");
                Serial.println(WiFi.status());
                Serial.println("[WIFI] Error codes: [1 = No network], [4 = Wrong password/Error], [6 = Disconnected]");
            }
            return;
        }
    }
}

bool WiFiMenager::send_json(uint8_t client_num, MachineStatus &data)
{
    static StaticJsonDocument<256> doc;
    doc.clear();
    JsonObject obj = doc.to<JsonObject>();
    data.to_json(obj);

    char buffer[256];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    
    return websocket.sendTXT(client_num, (uint8_t*)buffer, len);
}

void WiFiMenager::broadcast_telemetry(const char* json_data)
{
    websocket.broadcastTXT(json_data);
}

void WiFiMenager::handle_info_get()
{
    StaticJsonDocument<256> doc;
    doc["version"] = version;
    doc["date"] = date;
    doc["features"] = features;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WiFiMenager::set_info(String version, String date, String features)
{
    this->version = version;
    this->date = date;
    this->features = features;
}

int WiFiMenager::get_active_clients() { return active_clients; }