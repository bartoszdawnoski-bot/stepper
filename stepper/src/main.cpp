#include "gcode.h"
#include "SD_controller.h"
#include "Wifi_menger.h"
#include "MsgPack.h"
#include "packets.h"
#include "pins.h"

bool move_form_website = false;
int index_website = 0;

// Dane logowania do sieci WiFi
char TEST_SSID[] = "GIGA_COM_68B1";
char TEST_PASS[] = "ddEFmZ9U";

// Rozmiar buforów komunikacyjnych 
const int BUFFER_SIZE = 2;

// ________PAMIĘĆ WSPÓLNA________

// Kolejka komend G-Code (Wi-Fi [Core 1] -> Silniki [Core 0])
GcodeCom comandQueue[BUFFER_SIZE];
volatile int cmdhead = 0; // dla core 1
volatile int cmdtail = 0; // dla core 2

// Kolejka Feedback/ACK (Silniki [Core 0] -> Wi-Fi [Core 1])
MachineStatus ackQueue[BUFFER_SIZE];
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
MsgPack::Unpacker unpacker; // Do rozpakowywania danych przychodzących
MsgPack::Packer packer; // Do pakowania danych wychodzących

// Konfiguracja silników krokowych na wybranych pinach i PIO
Stepper motorA(PIO_SELECT, STEP_PIN, DIR_PIN, ENABLE_PIN ,HOLD_PIN);
Stepper motorB(PIO_SELECT, STEP_PIN2, DIR_PIN2, ENABLE_PIN2 ,HOLD_PIN2);

// Procesor G-Code sterujący silnikami
GCode procesor(&motorA, &motorB);

// Kontroler karty SD (obecnie nieużywany w głównej pętli, ale zainicjalizowany)
SDController sdController(CS_PIN, DD_PIN, CLK_PIN, CMD_PIN);

// Menedżer WiFi obsługujący WebSockets i mDNS
WiFiMenager wifi(TEST_SSID, TEST_PASS);

// ________CALLBACK WIFI________ 
// Funkcja wywoływana automatycznie, gdy przyjdą dane z sieci
void on_data_received(uint8_t num, uint8_t *payload, size_t length, WStype_t type) 
{
    // Czeaka tylko na dane binarne czyli MsgPack
    if (type == WStype_BIN) 
    {
        //dodawanie danych do unpackera 
        unpacker.feed(payload, length);
        GcodeCom packet;

        // Pętla deserializacji jej zadnie to wyciąganie wszystkich pełnych pakietów z bufora
        while(unpacker.deserialize(packet))
        {
            // Jeśli jest miejsce w kolejce do Core 0
            if(!is_cmd_full())
            {
                // Zapisz pakiet i przesuń wskaźnik zapisu
                comandQueue[cmdhead] = packet;
                cmdhead = ( ( cmdhead + 1 ) % BUFFER_SIZE );
            }
            else
            {
                // Błąd przepełnienia bufora pakiet jest tracony
                if(Serial) Serial.println("[Core 1] Buffer is full");
            }
        }
    }
    // Opcjonalnie obsługa tekstu
    else if (type == WStype_TEXT) 
    {
        String txt = String((char*)payload);
        if (num == 255 && !is_cmd_empty())
        {
            if(Serial) Serial.println("[Jog] Ignored - Machine Busy!");
            return; 
        }
        if(txt.startsWith("G")) 
        {
            GcodeCom packet;
            packet.Gcode = txt;  
            packet.id = 0;     
            packet.is_last = true;
            if(!is_cmd_full()) {
                move_form_website = true;
                comandQueue[cmdhead] = packet;
                index_website = cmdhead;
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
    while(!Serial){delay(10);} // Oczekiwanie na monitor portu szeregowego - do wyzucenia w wersji koncowej
    delay(2000); 

    // Inicjalizacja silników
    if(motorA.init() && motorB.init())
    {
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
        GcodeCom task = comandQueue[cmdtail];
        cmdtail = (cmdtail + 1) % BUFFER_SIZE;

        // Wykonaj ruch jeśli komenda nie jest pusta
        if(task.Gcode.length() > 0)
        {
            procesor.processLine(task.Gcode); // Parsowanie i uruchomienie ruchu
            procesor.move_complete();// blokowanie rdzenia az ruch sie nie skonczy 

            if(move_form_website)
            {
                cmdhead = index_website;
                move_form_website = false;
            }

        } 
        //po wykonaniu ruchu przygotowanie potweirdzenia ack
        if(!is_ack_full())
        {
            MachineStatus ack;
            
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
    // Sprawdź, czy Core 0 wystawił jakieś potwierdzenia do wysłania
    if(!is_ack_empty())
    {
        MachineStatus ack_to_send = ackQueue[acktail];
        acktail = (acktail + 1) % BUFFER_SIZE;
        // Wyślij potwierdzenie do PC przez WebSocket
        wifi.send_msgpack(ack_to_send, packer);
    }
}