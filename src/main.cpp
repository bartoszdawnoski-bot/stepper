#include "stepper.h"


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



 void setup() {
    Serial.begin(115200);
    while(!Serial){delay(10);}
    delay(2000); 
    Serial.println("--- Test Synchronizacji Silnikow PIO ---");

    if (!motorA.init() || !motorB.init()) 
    {
        Serial.println("BLAD: Nie udalo sie zainicjowac silnikow (PIO/SM).");
        while(1);
    }
    Serial.println("Silniki A i B zainicjowane poprawnie.");

    motorA.setSpeed(200.0f); 
    motorB.setSpeed(200.0f); 
    motorA.setEnable(true);
    motorB.setEnable(true);
}



void loop() {
    Serial.println("\n--- TEST 1: RUCH SYNCHRONICZNY ---");
    
    long stepsA = 400;
    long stepsB = -800; 
    motorA.setSteps(stepsA); 
    motorB.setSteps(stepsB); 

    Serial.print("Przygotowanie ruchu: A="); Serial.print(stepsA); 
    Serial.print(", B="); Serial.println(stepsB);

    Stepper::moveSteps();
    while (motorA.moving() || motorB.moving()) 
    {
    }
    Serial.println("Ruch synchroniczny zakonczony.");
    Serial.print("Poz. A: "); Serial.print(motorA.getPosition()); 
    Serial.print(", Poz. B: "); Serial.println(motorB.getPosition());

    delay(3000); 

    Serial.println("\n--- TEST 2: RUCH NIESYNCHRONICZNY (A solo) ---");

    motorA.setSteps(-2000);
    
    Serial.println("Silnik A (tylko) start. Czekam.");
    Stepper::moveSteps();
    while (motorA.moving()) 
    {
    }
    Serial.print("Ruch A zakonczony. Poz. A: "); Serial.println(motorA.getPosition());

    delay(1000);

    Serial.println("\n--- TEST 3: RUCH NIESYNCHRONICZNY (B solo) ---");
    motorB.setSteps(4000);
    
    Serial.println("Silnik B (tylko) start. Czekam.");
    Stepper::moveSteps();
    while (motorB.moving()) 
    {
    }
    Serial.print("Ruch B zakonczony. Poz. B: "); Serial.println(motorB.getPosition());

    Serial.println("\n--- Test Powtarzam ---");

    delay(5000); 
}