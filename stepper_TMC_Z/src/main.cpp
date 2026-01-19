#include "gcode.h"
#include "SD_controller.h"
#include "Wifi_menger.h"
#include <MsgPack.h>
#include "packets.h"
#include "conf.h"
#include <SPI.h>

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 32;

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
MsgPack::Packer packer; // Do pakowania danych wychodzących
MsgPack::Unpacker unpacker;

// Konfiguracja silników krokowych na wybranych pinach i PIO
Stepper motorA(PIO_SELECT_0, STEP_PIN_1, DIR_PIN_1, ENABLE_PIN_1 , HOLD_PIN_1);
Stepper motorB(PIO_SELECT_1, STEP_PIN_2, DIR_PIN_2, ENABLE_PIN_2 ,HOLD_PIN_2);
Stepper motorC(PIO_SELECT_2, STEP_PIN_3, DIR_PIN_3, ENABLE_PIN_3 ,HOLD_PIN_3);

// Procesor G-Code sterujący silnikami
GCode procesor(&motorA, &motorB, &motorC);

// Menedżer WiFi obsługujący WebSockets i mDNS
WiFiMenager wifi;

// ________CALLBACK WIFI________ 
// Funkcja wywoływana automatycznie, gdy przyjdą dane z sieci
void on_data_received(uint8_t num, uint8_t *payload, size_t length, WStype_t type) 
{
    // Czeaka tylko na dane binarne czyli MsgPack
    if (type == WStype_BIN) 
    {
         
        unpacker.feed(payload, length);
        processedData data_packet;
        // probujemy odpakowac
        if(unpacker.deserialize(data_packet))
        {
            // jak jest miejsce w kolejce to wrzucamy
            if(!is_cmd_full())
            {
                data_packet.client_num = num;
                comandQueue[cmdhead] = data_packet;
                cmdhead = (cmdhead + 1) % BUFFER_SIZE;
                Serial.print("[MsgPack] Packet added to queue: " ); Serial.println(data_packet.Gcode);
            }
            else
            {
                Serial.println("[Core 1] Buffer is full");
            }
        }
        else 
        {
            Serial.print("[MsgPack] Deserialization ERROR. Payload size: ");
            Serial.println(length);
        }
    }
    // Opcjonalnie obsługa tekstu 
    else if (type == WStype_TEXT) 
    {
        // Obsługa tekstu bez tworzenia globalnych Stringów
        char tempBuff[64];
        if (length > 63) length = 63;
        memcpy(tempBuff, payload, length);
        tempBuff[length] = '\0'; // Null-terminator

        if (tempBuff[0] == 'G') 
        {
             if(!is_cmd_full()) 
             {
                processedData packet;
                strncpy(packet.Gcode, tempBuff, 63);
                packet.Gcode[63] = '\0';
                packet.id = 0;     
                packet.is_last = true;
                packet.client_num = num;
                
                comandQueue[cmdhead] = packet;
                cmdhead = (cmdhead + 1) % BUFFER_SIZE;
            }
        }   
    }   
}

// ________CORE 0 STEROWANIE RUCHEM________
void setup() 
{
    Serial.begin(115200);
    while(!Serial || !wifi.isCon()){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
    delay(2000); 

    SPI.setRX(16);  // MISO
    SPI.setTX(19);  // MOSI
    SPI.setSCK(18); // SCK
     // SPI.setCS(TMC_CS_PIN_IGNORE);
    SPI.begin();

    pinMode(TMC_CS_PIN_IGNORE, OUTPUT);
    digitalWrite(TMC_CS_PIN_IGNORE, HIGH);
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
    // Sprawdzanie czy są nowe komendy od Core 1
    if(!is_cmd_empty())
    {
        // Pobieranie zadania z kolejki
        processedData task = comandQueue[cmdtail];
        cmdtail = (cmdtail + 1) % BUFFER_SIZE;

        // Wykonaj ruch jeśli komenda nie jest pusta
        if(task.Gcode[0] != '\n')
        {
            procesor.processLine(task.Gcode); // Parsowanie i uruchomienie ruchu
            procesor.move_complete();// blokowanie rdzenia az ruch sie nie skonczy
        } 

        if(procesor.is_em_stopped()) procesor.em_stopp();
    }   
    else
    {
        procesor.flush_buffer();
        delay(1);
    }
}

unsigned long last_status_time = 0;

void loop1() 
{
    wifi.run();
    
    if(procesor.is_em_stopped()) wifi.send_stop();

    // <<< ZMIANA: Heartbeat Status (co 200ms) zamiast ACK po każdym ruchu
    if (millis() - last_status_time > 200) 
    {
        last_status_time = millis();
        
        MachineStatus status;
        status.msgType = 0x01;
        status.id = 0;
        status.x = procesor.getX();
        status.y = procesor.getY();
        status.z = procesor.getZ();
        status.ack = true; 

        if (procesor.is_moving() || !is_cmd_empty()) strcpy(status.state, "Run");
        else strcpy(status.state, "Idle");
        
        wifi.broadcast_status(status, packer); 
    }

    if (wifi.config_changed)
    {
        float sx, sy, sz, st_mm, st_rot, st_br;
        if (wifi.load_config(sx, sy, sz, st_mm, st_rot, st_br)) {
            procesor.update_settings(sx, sy, sz, st_mm, st_rot, st_br);
        }
        wifi.config_changed = false;
    }
}