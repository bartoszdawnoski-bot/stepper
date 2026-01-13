#include "gcode.h"
#include "SD_controller.h"
#include "Wifi_menger.h"
#include <MsgPack.h>
#include "packets.h"
#include "conf.h"
#include <SPI.h>

// dane logowania wpisane na sztywno zeby szybko testowac
char TEST_SSID[] = "GIGA_COM_68B1";
char TEST_PASS[] = "ddEFmZ9U";

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 16;

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
Stepper motorA(PIO_SELECT_0, STEP_PIN_1, DIR_PIN_1, ENABLE_PIN_1 , HOLD_PIN_1, TRANSOPT_PIN);
Stepper motorB(PIO_SELECT_1, STEP_PIN_2, DIR_PIN_2, ENABLE_PIN_2 ,HOLD_PIN_2);

// Procesor G-Code sterujący silnikami
GCode procesor(&motorA, &motorB);

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
        GcodeCom packet;
        // probujemy odpakowac
        if(unpacker.deserialize(packet))
        {
            // jak jest miejsce w kolejce to wrzucamy
            if(!is_cmd_full())
            {
                processedData data_packet;
                data_packet.msgType = packet.msgType;
                data_packet.Gcode = packet.Gcode;
                data_packet.id = packet.id;
                data_packet.is_last = packet.is_last;
                data_packet.client_num = num;
                comandQueue[cmdhead] = data_packet;

                cmdhead = ( ( cmdhead + 1 ) % BUFFER_SIZE );
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
        String txt = String((char*)payload);
        if (num == 255 && !is_cmd_empty())
        {
            if(Serial) Serial.println("[Jog] Machine Busy!");
            return; 
        }
        if(txt.startsWith("G")) 
        {
            processedData packet;
            packet.Gcode = txt;  
            packet.id = 0;     
            packet.is_last = true;
            packet.client_num = num;
            
            if(!is_cmd_full()) {
                comandQueue[cmdhead] = packet;
                cmdhead = (cmdhead + 1) % BUFFER_SIZE;
                if(Serial) Serial.println("[Jog] Added to queue: " + txt);
            }
        }        
    }   
}

// ________CORE 0 STEROWANIE RUCHEM________
void setup() 
{
    Serial.begin(115200);
    while(!Serial || wifi.isCon()){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
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
    if(motorA.init() && motorB.init())
    {
        // Inicjalizacja sterowników
        motorA.initTMC(CS_PIN_A, R_SENSE_TMC_PLUS, CURRENT_A);
        motorB.initTMC(CS_PIN_B, R_SENSE_TMC_PLUS, CURRENT_B); 
        Serial.println("[Core 0] Steppers are ready");
    }
    else
    {
        Serial.println("[Core 0] restart the winder or check the motors");
        while(true) { delay(1); } // Zatrzymaj program w przypadku błędu
    }
    motorA.setEnable(true);
    motorB.setEnable(true);
}
// ________CORE 1 KOMUNIKACJA WIFI________
void setup1()
{
    // Próba połączenia z WiFi i uruchomienia mDNS
    while(!Serial){delay(10);}
    while(!wifi.init()) { delay(1); }
    float sx = 100.0f, sy = 200.0f, st_mm = 100.0f, st_rot = 200.0f;

    if (wifi.load_config(sx, sy, st_mm, st_rot)) 
    {
        Serial.println("[Config] Settings loaded from config.json");    
        procesor.update_settings(sx, sy, st_mm, st_rot);
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
        if(task.Gcode.length() > 0)
        {
            procesor.processLine(task.Gcode); // Parsowanie i uruchomienie ruchu
            procesor.move_complete();// blokowanie rdzenia az ruch sie nie skonczy
        } 
        //po wykonaniu ruchu przygotowanie potweirdzenia ack
        if(!is_ack_full())
        {
            processedStatus ack;
            
            ack.id = task.id;
            ack.state = "OK";
            ack.target_client = task.client_num;
            ack.ack = true;

            ackQueue[ackhead] = ack;
            ackhead = (ackhead + 1) % BUFFER_SIZE;
        }
    }   
}

void loop1() 
{
    // Obsługa stosu sieciowego czyli utrzymanie połączenia, ping-pong, odbiór danych
    wifi.run();
    if(procesor.is_em_stopped())
    {
        wifi.send_stop();
    }
    //sprawdzenie zmiany ustawien 
    if (wifi.config_changed)
    {
        // Zmienne tymczasowe do wczytania nowych wartości
        float sx, sy, st_mm, st_rot;
        
        // Próba wczytania nowego pliku
        if (wifi.load_config(sx, sy, st_mm, st_rot)) 
        {
            // Aktualizacja ustawień w obiekcie GCode
            procesor.update_settings(sx, sy, st_mm, st_rot);
            Serial.println("[Config] Settings loaded from config.json");
        }
        
        // Reset flagi, żeby nie wczytywać w kółko
        wifi.config_changed = false;
    }

    // Sprawdź, czy Core 0 wystawił jakieś potwierdzenia do wysłania
    if(!is_ack_empty())
    {
        MachineStatus ack_to_send; 
        ack_to_send.ack = ackQueue[acktail].ack;
        ack_to_send.id = ackQueue[acktail].id;
        ack_to_send.msgType = ackQueue[acktail].msgType;
        ack_to_send.state = ackQueue[acktail].state;
        // Wyślij potwierdzenie do PC przez WebSocket
        if(ackQueue[acktail].target_client != 255)
        {
            wifi.send_msgpack(ackQueue[acktail].target_client , ack_to_send, packer);
        }
        acktail = (acktail + 1) % BUFFER_SIZE;
    }
}

