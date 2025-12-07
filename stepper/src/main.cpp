#include "gcode.h"

PIO pio = pio0; 

#define ENABLE_PIN 5
#define STEP_PIN  1
#define DIR_PIN   0 
#define HOLD_PIN  6

#define ENABLE_PIN2 9
#define STEP_PIN2  8
#define DIR_PIN2   7 
#define HOLD_PIN2  10


Stepper motorA(pio, STEP_PIN, DIR_PIN, ENABLE_PIN ,HOLD_PIN);
Stepper motorB(pio, STEP_PIN2, DIR_PIN2, ENABLE_PIN2 ,HOLD_PIN2);

GCode procesor(&motorA, &motorB);

String inputString = "";

void setup() {
    Serial.begin(115200);
    while(!Serial){delay(10);}
    delay(2000); 
    Serial.println("--- G-CODE TESTER---");

    if (!motorA.init() || !motorB.init()) 
    {
        Serial.println("BLAD: Nie udalo sie zainicjowac silnikow (PIO/SM).");
        while(1);
    }
    Serial.println("Silniki A i B zainicjowane poprawnie.");
    Serial.println("Wózek: X [mm] | Wrzeciono: Y/C [obroty]");
    Serial.println("Wpisz komendę (np. G1 X10 F5 Y2.5)");
    motorA.setEnable(true);
    motorB.setEnable(true);
}

void loop() {
    while (Serial.available()) 
    {
        char inChar = (char)Serial.read();
        inputString += inChar;
        if (inChar == '\n' || inChar == '\r') 
        {
            inputString.trim(); 
            if (inputString.length() > 0) 
            {
                
                Serial.print("Odebrano: ");
                Serial.println(inputString);
                procesor.processLine(inputString);
                procesor.move_complete();
            }
            inputString = "";
        }
    }
}