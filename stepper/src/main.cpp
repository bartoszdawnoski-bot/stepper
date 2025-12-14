#include "gcode.h"
#include "SD_controller.h"
#include "Wifi_menger.h"
#include "MsgPack.h"
#include "packets.h"
#include "pins.h"

//przypisanie danych dla wifi
char TEST_SSID[] = "Internet";
char TEST_PASS[] = "12345678";

//konfiguracja bufforow 
const int BUFFER_SIZE = 1;

//dla Gcodu (wifi -> silniki)
GcodeCom comandQueue[BUFFER_SIZE];
volatile int cmdhead = 0; // dla core 1
volatile int cmdtail = 0; // dla core 2

//feedback (silniki -> wifi)
MachineStatus ackQueue[BUFFER_SIZE];
volatile int ackhead = 0; // dla core 1
volatile int acktail = 0; // dla core 2

//funkcje pomocnicze dal bufforow
bool is_cmd_full() { return ((cmdhead + 1) % BUFFER_SIZE) == cmdtail; }
bool is_ack_full() { return ((ackhead + 1) % BUFFER_SIZE) == acktail; }

bool is_cmd_empty() { return cmdhead == cmdtail; }
bool is_ack_empty() { return ackhead == acktail; }

//deklaracje obiektow
MsgPack::Unpacker unpacker;
MsgPack::Packer packer;
Stepper motorA(PIO_SELECT, STEP_PIN, DIR_PIN, ENABLE_PIN ,HOLD_PIN);
Stepper motorB(PIO_SELECT, STEP_PIN2, DIR_PIN2, ENABLE_PIN2 ,HOLD_PIN2);
GCode procesor(&motorA, &motorB);
SDController sdController(CS_PIN, DD_PIN, CLK_PIN, CMD_PIN);
WiFiMenager wifi(TEST_SSID, TEST_PASS);

//callback wifi
void on_data_received(uint8_t *payload, size_t length, WStype_t type) {
    if (type == WStype_BIN) {
        unpacker.feed(payload, length);
        GcodeCom packet;

        while(unpacker.deserialize(packet))
        {
            if(!is_cmd_full())
            {
                comandQueue[cmdhead] = packet;
                cmdhead = ( ( cmdhead + 1 ) % BUFFER_SIZE );
            }
            else
            {
                if(Serial) Serial.println("[Core 1] Buffer is full");
            }
        }
    }
}

void setup() 
{
    Serial.begin(115200);
    while(!Serial){delay(10);}
    delay(2000); 
    if(motorA.init() && motorB.init())
    {
        Serial.println("[Core 0] Steppers are ready");
    }
    else
    {
        Serial.println("[Core 0] restart the winder or check the motors");
        while(true) { delay(1); }
    }
}

void setup1()
{
    while(!wifi.init()) { delay(1); }
    wifi.set_callback(on_data_received);
    Serial.println("[Core 1] WIFI ready");
}

void loop() 
{
    if(!is_cmd_empty)
    {
        GcodeCom task = comandQueue[cmdtail];
        cmdtail = (cmdtail + 1) % BUFFER_SIZE;
        if(task.Gcode.length() > 0)
        {
            procesor.processLine(task.Gcode);
            procesor.move_complete();
        } 
        if(!is_ack_full)
        {
            MachineStatus ack;
            //przypisac wartosci do wyslania !!
            ackQueue[ackhead] = ack;
            ackhead = (ackhead + 1) % BUFFER_SIZE;
        }
    }   
}

void loop1() 
{
    wifi.run();
    if(!is_ack_empty)
    {
        MachineStatus ack_to_send = ackQueue[acktail];
        acktail = (acktail + 1) % BUFFER_SIZE;
        wifi.send_msgpack(ack_to_send, packer);
    }
}