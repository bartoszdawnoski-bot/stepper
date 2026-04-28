#include "gcode.h"
#include "Wifi_menger.h"
#include <ArduinoJson.h>
#include "packets.h"
#include "conf.h"
#include <SPI.h>
#include <malloc.h>
#include "pico/mutex.h"

#define FIRMWARE_VERSION "v1.0.0"
#define FIRMWARE_DATE __DATE__ " " __TIME__ 
#define FIRMWARE_AUTHOR "Nawijarka CNC Project"
#define FIRMWARE_FEATURES "TMC5160 SPI, PIO Steppers, WebSockets, G-Code Parser"

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 512;
bool core1_separate_stack = true;

//Uzywane zmienne
const int BATCH_SIZE = 15; 
int processed_count = 0;
int global_packet_counter = 0;
unsigned long last_wifi_ping = 0; 
unsigned long last_telemetry = 0;
float last_posX = 0;
volatile bool e_stop_triggered_isr = false;
static bool stop_sent = false; 

// ________PAMIĘĆ WSPÓLNA________

// Kolejka komend G-Code (Wi-Fi [Core 1] -> Silniki [Core 0])
processedData comandQueue[BUFFER_SIZE];
volatile int cmdhead = 0; // dla core 1
volatile int cmdtail = 0; // dla core 2

// Kolejka Feedback/ACK (Silniki [Core 0] -> Wi-Fi [Core 1])
processedStatus ackQueue[BUFFER_SIZE];
volatile int ackhead = 0; // dla core 1
volatile int acktail = 0; // dla core 2

// funkcje pomocnicze dal bufforow
// Sprawdza, czy następny krok głowy nie nadpisze ogona
bool is_cmd_full() { return ((cmdhead + 1) % BUFFER_SIZE) == cmdtail; }
bool is_ack_full() { return ((ackhead + 1) % BUFFER_SIZE) == acktail; }

// Sprawdza, czy głowa i ogon są w tym samym miejscu
bool is_cmd_empty() { return cmdhead == cmdtail; }
bool is_ack_empty() { return ackhead == acktail; }
// ________DEKLARACJE OBIEKTÓW________

// Konfiguracja silników krokowych na wybranych pinach i PIO
Stepper motorA(PIO_SELECT_0, STEP_PIN_1, DIR_PIN_1, ENABLE_PIN_1 , HOLD_PIN_1);
Stepper motorB(PIO_SELECT_1, STEP_PIN_2, DIR_PIN_2, ENABLE_PIN_2 ,HOLD_PIN_2);
Stepper motorC(PIO_SELECT_2, STEP_PIN_3, DIR_PIN_3, ENABLE_PIN_3 ,HOLD_PIN_3);

// Procesor G-Code sterujący silnikami
GCode procesor(&motorA, &motorB, &motorC);

// Menedżer WiFi obsługujący WebSockets i mDNS
WiFiMenager wifi;

//Mutex
mutex_t cmd_mutex;
mutex_t ack_mutex;

void e_stop_isr()
{
    digitalWrite(ENABLE_PIN_1, HIGH);
    digitalWrite(ENABLE_PIN_2, HIGH);
    digitalWrite(ENABLE_PIN_3, HIGH);

    e_stop_triggered_isr = true;
    procesor.em_stopp(); 
}

void printMemoryDebug(int counter) {
    struct mallinfo m = mallinfo();
    
    uint32_t free_ram = m.fordblks; 
    
    Serial.print("[MEM] Pkt: ");
    Serial.print(counter);
    Serial.print(" | Wolny RAM: ");
    Serial.print(free_ram);
    Serial.println(" bajtów");
}

// ________CALLBACK WIFI________ 
// Funkcja wywoływana automatycznie, gdy przyjdą dane z sieci
void on_data_received(uint8_t num, uint8_t *payload, size_t length, WStype_t type) 
{
    if (type == WStype_TEXT) 
    {
        global_packet_counter++;
        if (global_packet_counter % 100 == 0) printMemoryDebug(global_packet_counter);


        String msg = String((char*)payload);
        if (msg.startsWith("OVERRIDE:")) 
        {
            float val = msg.substring(9).toFloat() / 100.0f; 
            Stepper::set_global_override(val);
            
            if(Serial) { 
                Serial.print("[WIFI] Zmieniono Override na: "); 
                Serial.println(val); 
            }
            return; 
        }

        if (payload[0] == 'G' || payload[0] == 'M') 
        {
            processedData packet;
            
            size_t copy_len = length;
            if(copy_len >= MAX_GCODE_LEN) copy_len = MAX_GCODE_LEN - 1;
            
            while (copy_len > 0 && (payload[copy_len - 1] == '\n' || payload[copy_len - 1] == '\r')) {
                copy_len--;
            }
            
            memcpy(packet.Gcode, payload, copy_len);
            packet.Gcode[copy_len] = '\0';  
            
            packet.id = 0;     
            packet.is_last = true;
            packet.client_num = num;
            
            mutex_enter_blocking(&cmd_mutex);
            if(!is_cmd_full()) {
                comandQueue[cmdhead] = packet;
                cmdhead = (cmdhead + 1) % BUFFER_SIZE;
            }
            mutex_exit(&cmd_mutex);
            
            if(Serial) { Serial.print("[WIFI] Otrzymano G-CODE: "); Serial.println(packet.Gcode); }
            return;
        }

        static StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error) {
            if(Serial)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
            }
            return; 
        }

        if(!is_cmd_full())
        {
            GcodeCom packet;
            packet.from_json(doc.as<JsonObject>()); // Konwersja JSON -> struct

            processedData data_packet;
            data_packet.msgType = packet.msgType;
            
            strncpy(data_packet.Gcode, packet.Gcode, MAX_GCODE_LEN);
            data_packet.Gcode[MAX_GCODE_LEN - 1] = '\0';
            
            data_packet.id = packet.id;
            data_packet.is_last = packet.is_last;
            data_packet.client_num = num;
            
            mutex_enter_blocking(&cmd_mutex);
            comandQueue[cmdhead] = data_packet;
            cmdhead = ( ( cmdhead + 1 ) % BUFFER_SIZE );
            mutex_exit(&cmd_mutex);
        }
        else
        {
             if(Serial) Serial.println("[Core 1] CMD Buffer Full");
        }
    }
}
// CORE 0 STEROWANIE RUCHEM
void setup() 
{
    mutex_init(&cmd_mutex);
    mutex_init(&ack_mutex);

    Serial.begin(115200);
    while(!wifi.isCon()){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
    delay(2000); 

    Serial.println("\n=============================================");
    Serial.println("       " FIRMWARE_AUTHOR "       ");
    Serial.print(" Wersja:     "); Serial.println(FIRMWARE_VERSION);
    Serial.print(" Kompilacja: "); Serial.println(FIRMWARE_DATE);
    Serial.print(" Funkcje:    "); Serial.println(FIRMWARE_FEATURES);
    Serial.println("=============================================\n");

    SPI.setRX(TMC_MISO_PIN);  // MISO
    SPI.setTX(TMC_MOSI_PIN);  // MOSI
    SPI.setSCK(TMC_SCK_PIN); // SCK
    SPI.setCS(TMC_CS_PIN_IGNORE);
    SPI.begin();

    pinMode(TMC_CS_PIN_IGNORE, OUTPUT);
    digitalWrite(TMC_CS_PIN_IGNORE, HIGH);
    pinMode(TRANSOPT_PIN_A, OUTPUT);
    pinMode(TRANSOPT_PIN_B, OUTPUT);
    pinMode(E_STOP_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(E_STOP_PIN), e_stop_isr, RISING);
    // Inicjalizacja silników
    if(motorA.init() && motorB.init() && motorC.init())
    {
        // Inicjalizacja sterowników
        motorA.initTMC(CS_PIN_A, R_SENSE_TMC_PLUS, CURRENT_A);
        motorB.initTMC(CS_PIN_B, R_SENSE_TMC_PRO, CURRENT_B); 
        motorC.initTMC(CS_PIN_C, R_SENSE_TMC_PRO, CURRENT_C);
        Serial.println("[Core 0] Steppers are ready");
    }
    else
    {
        Serial.println("[Core 0] Restart the winder or check the motors");
        while(true) { delay(1); } // Zatrzymaj program w przypadku błędu
    }
    motorA.setEnable(true);
    motorB.setEnable(true);
    motorC.setEnable(true);
}
// CORE 1 KOMUNIKACJA WIFI
void setup1()
{
    int reconnect_counter = 0;
    // Próba połączenia z WiFi i uruchomienia mDNS
    while(!wifi.init() && reconnect_counter <= 8) 
    { 
        delay(1);
        reconnect_counter++; 
    }

    if(!wifi.isCon())
    {
        wifi.ap_wizard();
    }

    float sx = 200.0f, sy = 200.0f, maxz = 200.0f ,st_mm = 200.0f, st_rot = 200.0f;
    if (wifi.load_config(sx, sy, maxz, st_mm, st_rot)) 
    {
        Serial.println("[Config] Settings loaded from config.json");    
        procesor.update_settings(sx, sy, maxz, st_mm, st_rot);
    } 
    else 
    {
        Serial.println("[Config] Failed to load config, using defaults.");
    }

    // Rejestracja funkcji callback do obsługi przychodzących danych
    wifi.set_callback(on_data_received);
    Serial.println("[Core 1] WIFI ready");
}

void loop() 
{   
    static bool estop_handled = false;
    if (e_stop_triggered_isr && !estop_handled)
    {
        motorA.e_stop();
        motorB.e_stop();
        motorC.e_stop();
        estop_handled = true;
        
        Serial.println("[MAIN] E-STOP ACTIVATED! Motors disabled.");
    }

    if(digitalRead(E_STOP_PIN) == LOW && procesor.is_em_stopped() && estop_handled) 
    {
        delay(50); 
        if(digitalRead(E_STOP_PIN) == LOW) 
        {
            Serial.println("[MAIN] E-STOP RELEASED. Ready.");
            procesor.em_stopp_f(); 
            e_stop_triggered_isr = false;
            estop_handled = false;
        }
    }

    processedData task;
    if(!procesor.is_em_stopped() && !is_cmd_empty() && !is_ack_full())
    {
        mutex_enter_blocking(&cmd_mutex);
        task = comandQueue[cmdtail];
        cmdtail = (cmdtail + 1) % BUFFER_SIZE;
        mutex_exit(&cmd_mutex);

        if(strnlen(task.Gcode, MAX_GCODE_LEN) > 0)
        {
            procesor.processLine(task.Gcode, !is_cmd_empty()); 
        } 
        
        processedStatus ack;
        ack.msgType = task.msgType;
        ack.id = task.id;
        strncpy(ack.state, "OK", STATE_LEN);
        ack.target_client = task.client_num;
        ack.ack = true;

        mutex_enter_blocking(&ack_mutex);
        ackQueue[ackhead] = ack;
        ackhead = (ackhead + 1) % BUFFER_SIZE;
        mutex_exit(&ack_mutex);
    }
}

void loop1() 
{
    if(wifi.isCon()) wifi.run(); 
    else wifi.reconnect(); 
    if(procesor.is_em_stopped()) 
    {
        if(!stop_sent) 
        {
            wifi.send_stop();
            stop_sent = true;
        }
    }
    else 
    {
        stop_sent = false;
    }


    if (wifi.config_changed)
    {
        float sx, sy, maxz, st_mm, st_rot;
        
        if (wifi.load_config(sx, sy, maxz, st_mm, st_rot)) 
        {
            procesor.update_settings(sx, sy, maxz, st_mm, st_rot);
            Serial.println("[Config] Settings loaded from config.json");
        }
        
        wifi.config_changed = false;
    }

    if (millis() - last_telemetry > 500) 
    {
        unsigned long now = millis();
        float dt = (now - last_telemetry) / 1000.0f; 
        last_telemetry = now;
        
        StaticJsonDocument<512> doc;
        doc["msgType"] = 0x10; 
        
        float posX = (float)motorA.getPosition() / (100.0f * motorA.get_microsteps()); 
        doc["posX"] = posX;
        
        doc["velX"] = (posX - last_posX) / dt; 
        last_posX = posX; 
 
        float posY = (float)motorB.getPosition() / (200.0f * motorB.get_microsteps());
        doc["posY"] = posY * 360.0f; 
        
        doc["loadX"] = motorA.get_load(); 
        doc["loadY"] = motorB.get_load();

        doc["maxValZ"] = procesor.get_current_Z();
        doc["valZ"] = motorC.get_actuall_current();
        doc["motorsOn"] = motorA.isEnabled();
        
        doc["clients"] = wifi.get_active_clients();

        if (procesor.is_em_stopped()) doc["state"] = "E-STOP";
        else if (motorA.moving() || motorB.moving()) doc["state"] = "RUN";
        else doc["state"] = "IDLE";
        
        char buffer[512];
        serializeJson(doc, buffer);
        wifi.broadcast_telemetry(buffer); 
    }

    processed_count = 0;
    while(!is_ack_empty() && processed_count < BATCH_SIZE)
    {
        MachineStatus ack_to_send; 
        uint8_t current_target_client;

        mutex_enter_blocking(&ack_mutex);
        ack_to_send.ack = ackQueue[acktail].ack;
        ack_to_send.id = ackQueue[acktail].id;
        ack_to_send.msgType = ackQueue[acktail].msgType;
        strncpy(ack_to_send.state, ackQueue[acktail].state, STATE_LEN);
        current_target_client = ackQueue[acktail].target_client;
        mutex_exit(&ack_mutex);
        
        bool success = true;
       if(current_target_client != 255)
        {
            success = wifi.send_json(ackQueue[acktail].target_client , ack_to_send);   
        }
            
        if(!success) 
        {
            if(Serial) Serial.println("[WIFI] TX Buffer Full");
            break;
        } 

        mutex_enter_blocking(&ack_mutex);
        acktail = (acktail + 1) % BUFFER_SIZE;
        processed_count++;
        mutex_exit(&ack_mutex);
        
    }
}
