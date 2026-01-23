#include "gcode.h"
#include "Wifi_menger.h"
#include <ArduinoJson.h>
#include "packets.h"
#include "conf.h"
#include <SPI.h>
#include <malloc.h>

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 256;
bool core1_separate_stack = true;

//Uzywane zmienne
unsigned long last_wifi_retry = 0;
const int BATCH_SIZE = 15; 
int processed_count = 0;
int global_packet_counter = 0;
unsigned long last_wifi_ping = 0; 

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

        // Parsowanie JSON
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload, length);

        if (error) {
            if(Serial) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
            }
            
            // Fallback: Jeśli to nie JSON, sprawdź czy to surowy G-Code (np. z prostego terminala)
            if (payload[0] == 'G' || payload[0] == 'M') 
            {
                // ... Stary kod obsługi surowego tekstu ...
                processedData packet;
                size_t copy_len = (length < MAX_GCODE_LEN) ? length : (MAX_GCODE_LEN - 1);
                memcpy(packet.Gcode, payload, copy_len);
                packet.Gcode[copy_len] = '\0';  
                packet.id = 0;     
                packet.is_last = true;
                packet.client_num = num;
                
                if(!is_cmd_full()) {
                    comandQueue[cmdhead] = packet;
                    cmdhead = (cmdhead + 1) % BUFFER_SIZE;
                }
            }
            return;
        }

        // Jeśli JSON poprawny
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
            
            comandQueue[cmdhead] = data_packet;
            cmdhead = ( ( cmdhead + 1 ) % BUFFER_SIZE );
        }
        else
        {
             if(Serial) Serial.println("[Core 1] CMD Buffer Full");
        }
    }
}
// ________CORE 0 STEROWANIE RUCHEM________
void setup() 
{
    Serial.begin(115200);
    while(!Serial || !wifi.isCon()){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
    delay(2000); 

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

    // Inicjalizacja silników
    if(motorA.init() && motorB.init() && motorC.init())
    {
        // Inicjalizacja sterowników
        motorA.initTMC(CS_PIN_A, R_SENSE_TMC_PLUS, CURRENT_A);
        motorB.initTMC(CS_PIN_B, R_SENSE_TMC_PLUS, CURRENT_B); 
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
// ________CORE 1 KOMUNIKACJA WIFI________
void setup1()
{
    // Próba połączenia z WiFi i uruchomienia mDNS
    while(!Serial){delay(10);}
    while(!wifi.init()) { delay(1); }
    float sx = 200.0f, sy = 200.0f, sz = 200.0f ,st_mm = 200.0f, st_rot = 200.0f, st_br = 200.0f;

    if (wifi.load_config(sx, sy, sz, st_mm, st_rot, st_br)) 
    {
        Serial.println("[Config] Settings loaded from config.json");    
        procesor.update_settings(sx, sy, sz, st_mm, st_rot, st_br);
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
   if(digitalRead(E_STOP_PIN) == HIGH) 
   { 
        if(!procesor.is_em_stopped()) 
        {
            Serial.println("[MAIN] EMERGENCY STOP TRIGGERED!");
            motorA.e_stop();
            motorB.e_stop();
            motorC.e_stop();
            procesor.em_stopp();
            
            cmdhead = 0;
            cmdtail = 0;
            ackhead = 0;
            acktail = 0;
        }
    }

    if(procesor.is_em_stopped())
    {
            cmdhead = 0;
            cmdtail = 0;
            ackhead = 0;
            acktail = 0;
    }

   if(digitalRead(E_STOP_PIN) == LOW && procesor.is_em_stopped()) 
   {
        Serial.println("[MAIN] E-STOP RELEASED. ");
        procesor.em_stopp_f(); 
    }

    // Sprawdzanie czy są nowe komendy od Core 1
   if(!procesor.is_em_stopped() && !is_cmd_empty() && !is_ack_full())
    {
        // 1. Pobranie zadania
        processedData task = comandQueue[cmdtail];
        cmdtail = (cmdtail + 1) % BUFFER_SIZE;

        // 2. Wykonanie zadania (To może być szybkie G90 lub wolne G1)
        if(strnlen(task.Gcode, MAX_GCODE_LEN) > 0)
        {
            procesor.processLine(task.Gcode); 
        } 
        
        // 3. Wysłanie ACK (Mamy GWARANCJĘ miejsca, bo sprawdziliśmy to w ifie wyżej)
        processedStatus ack;
        ack.msgType = task.msgType;
        ack.id = task.id;
        strncpy(ack.state, "OK", STATE_LEN);
        ack.target_client = task.client_num;
        ack.ack = true;

        ackQueue[ackhead] = ack;
        ackhead = (ackhead + 1) % BUFFER_SIZE;
    }
}

void loop1() 
{
    // Obsługa stosu sieciowego czyli utrzymanie połączenia, ping-pong, odbiór danych
    if(wifi.isCon())
    {
        wifi.run(); 
        last_wifi_retry = 0; 
    }
    else
    {
        
        if(millis() - last_wifi_retry > 5000)
        {
            last_wifi_retry = millis();
            wifi.reconnect(); 
        }
    }

    if(procesor.is_em_stopped())
    {
        wifi.send_stop();
    }

    //sprawdzenie zmiany ustawien 
    if (wifi.config_changed)
    {
        // Zmienne tymczasowe do wczytania nowych wartości
        float sx, sy, sz, st_mm, st_rot, st_br;
        
        // Próba wczytania nowego pliku
        if (wifi.load_config(sx, sy, sz, st_mm, st_rot, st_br)) 
        {
            // Aktualizacja ustawień w obiekcie GCode
            procesor.update_settings(sx, sy, sz, st_mm, st_rot, st_br);
            Serial.println("[Config] Settings loaded from config.json");
        }
        
        // Reset flagi, żeby nie wczytywać w kółko
        wifi.config_changed = false;
    }

    // Sprawdź, czy Core 0 wystawił jakieś potwierdzenia do wysłania
    processed_count = 0;
    while(!is_ack_empty() && processed_count < BATCH_SIZE)
    {
        MachineStatus ack_to_send; 
        ack_to_send.ack = ackQueue[acktail].ack;
        ack_to_send.id = ackQueue[acktail].id;
        ack_to_send.msgType = ackQueue[acktail].msgType;
        strncpy(ack_to_send.state, ackQueue[acktail].state, STATE_LEN);
        
        bool success = true;
        // Wyślij potwierdzenie do PC przez WebSocket
       if(ackQueue[acktail].target_client != 255)
        {
            success = wifi.send_json(ackQueue[acktail].target_client , ack_to_send);   
        }
            
        if(!success) 
        {
            if(Serial) Serial.println("[WIFI] TX Buffer Full");
            break;
        } 

        acktail = (acktail + 1) % BUFFER_SIZE;
        processed_count++;

        if(millis() - last_wifi_ping > 500)
        {
            last_wifi_ping = millis();
            wifi.run(); 
        }
        
    }
}

