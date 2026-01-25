#include "gcode.h"
#include "Wifi_menger.h"
#include <ArduinoJson.h>
#include "packets.h"
#include "conf.h"
#include <SPI.h>
#include <malloc.h>
#include "pico/util/queue.h"

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 256;
bool core1_separate_stack = true;

//Uzywane zmienne
unsigned long last_wifi_retry = 0;
const int BATCH_SIZE = 15; 
int processed_count = 0;
int global_packet_counter = 0;
unsigned long last_wifi_ping = 0; 
volatile bool e_stop_triggered_isr = false;

// ________PAMIĘĆ WSPÓLNA________

queue_t cmd_queue;  // Kolejka komend (WiFi -> Silniki)
queue_t ack_queue;  // Kolejka potwierdzeń (Silniki -> WiFi)

// ________DEKLARACJE OBIEKTÓW________

// Konfiguracja silników krokowych na wybranych pinach i PIO
Stepper motorA(PIO_SELECT_0, STEP_PIN_1, DIR_PIN_1, ENABLE_PIN_1 , HOLD_PIN_1);
Stepper motorB(PIO_SELECT_1, STEP_PIN_2, DIR_PIN_2, ENABLE_PIN_2 ,HOLD_PIN_2);
Stepper motorC(PIO_SELECT_2, STEP_PIN_3, DIR_PIN_3, ENABLE_PIN_3 ,HOLD_PIN_3);

// Procesor G-Code sterujący silnikami
GCode procesor(&motorA, &motorB, &motorC);

// Menedżer WiFi obsługujący WebSockets i mDNS
WiFiMenager wifi;

void e_stop_isr()
{
    static unsigned long last_interrupt_time = 0;
    unsigned long interrupt_time = millis();
    
    if (interrupt_time - last_interrupt_time > 50) 
    {
        if(digitalRead(E_STOP_PIN) == HIGH) 
        {
            motorA.e_stop();
            motorB.e_stop();
            motorC.e_stop();
            procesor.em_stopp(); 
            e_stop_triggered_isr = true;
        }
    }
    last_interrupt_time = interrupt_time;
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
                
                bool added = queue_try_add(&cmd_queue, &packet);
                if(!added)
                {
                    if(Serial) Serial.println("[Core 1] CMD Buffer Full");
                }
            }
            return;
        }

        GcodeCom packet;
        packet.from_json(doc.as<JsonObject>()); // Konwersja JSON -> struct

        processedData data_packet;
        data_packet.msgType = packet.msgType;
            
        strncpy(data_packet.Gcode, packet.Gcode, MAX_GCODE_LEN);
        data_packet.Gcode[MAX_GCODE_LEN - 1] = '\0';
            
        data_packet.id = packet.id;
        data_packet.is_last = packet.is_last;
        data_packet.client_num = num;
            
        bool added = queue_try_add(&cmd_queue, &data_packet);
        if(!added)
        {
            if(Serial) Serial.println("[Core 1] CMD Buffer Full");
        }
    }
}
// ________CORE 0 STEROWANIE RUCHEM________
void setup() 
{
    Serial.begin(115200);
    while(!wifi.isCon()){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
    delay(2000); 

    queue_init(&cmd_queue, sizeof(processedData), BUFFER_SIZE);
    queue_init(&ack_queue, sizeof(processedStatus), BUFFER_SIZE);

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
   if (e_stop_triggered_isr) {
        Serial.println("[MAIN] EMERGENCY STOP TRIGGERED (ISR)!");
        e_stop_triggered_isr = false; 
    }

   if(digitalRead(E_STOP_PIN) == LOW && procesor.is_em_stopped()) 
   {
        Serial.println("[MAIN] E-STOP RELEASED. ");
        procesor.em_stopp_f(); 
    }

    processedData task;
    // Sprawdzanie czy są nowe komendy od Core 1
    if(!procesor.is_em_stopped() && queue_try_remove(&cmd_queue, &task))
    {
        if(strnlen(task.Gcode, MAX_GCODE_LEN) > 0)
        {
            procesor.processLine(task.Gcode); 
        } 
        processedStatus ack;
        ack.msgType = task.msgType;
        ack.id = task.id;
        strncpy(ack.state, "OK", STATE_LEN);
        ack.target_client = task.client_num;
        ack.ack = true;

        if(!queue_try_add(&ack_queue, &ack)) 
        {
            Serial.println("[Core 0] ACK Queue Full");
        }
    }
}
static processedStatus pending_ack;
static bool has_pending_ack = false;
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

    if (!has_pending_ack) 
    {
        if (queue_try_remove(&ack_queue, &pending_ack)) 
        {
            has_pending_ack = true;
        }
    }
    if (has_pending_ack)
    {
        if (pending_ack.target_client == 255) 
        {
             has_pending_ack = false; 
        }
        else 
        {
            MachineStatus ack_to_send; 
            ack_to_send.ack = pending_ack.ack; 
            ack_to_send.id = pending_ack.id;
            ack_to_send.msgType = pending_ack.msgType;
            strncpy(ack_to_send.state, pending_ack.state, STATE_LEN);

            bool success = wifi.send_json(pending_ack.target_client, ack_to_send);

            if (success) 
            {
                has_pending_ack = false;
                delay(0); 
            } 
            else 
            {
                Serial.println("[WIFI] Buffer Full"); 
            }
        }
    }
}
