#include "stepper.h"
#include "gcode.h"

PIO pio = pio2; 

#define ENABLE_PIN 5
#define STEP_PIN  1
#define DIR_PIN   0 
#define HOLD_PIN  16

#define ENABLE_PIN2 7
#define STEP_PIN2  8
#define DIR_PIN2   9 
#define HOLD_PIN2  17


Stepper motor1(pio, STEP_PIN, DIR_PIN, ENABLE_PIN ,HOLD_PIN);
Stepper motor2(pio, STEP_PIN2, DIR_PIN2, ENABLE_PIN2 ,HOLD_PIN2);

const char* testGCodeLines[] = {
    "G21 ; Jednostki w mm",
    "G91 ; Pozycjonowanie absolutne",
    "F500 ; Prędkość posuwu (Feed Rate)",
    "; --- START WARSTWY 1 ---",
    "G0 X10.5000 Y-13.5000 Z0.0000",
    "G1 X10.4948 Y-13.4956 Z0.3298",
    "G1 X10.4793 Y-13.4913 Z0.6593",
    "G1 X10.4534 Y-13.4869 Z0.9881",
    "G1 X10.4172 Y-13.4826 Z1.3160",
    "G1 X10.3707 Y-13.4782 Z1.6426",
    "G1 X10.3140 Y-13.4739 Z1.9675",
    "G1 X10.2471 Y-13.4695 Z2.2905",
    "G1 X10.1701 Y-13.4652 Z2.6112",
    "G1 X10.0831 Y-13.4608 Z2.9294",
    "G1 X9.9861 Y-13.4565 Z3.2447",
    "G1 X9.8792 Y-13.4521 Z3.5567",
    "G1 X9.7627 Y-13.4477 Z3.8653",
    "G1 X9.6364 Y-13.4434 Z4.1701",
    "G1 X9.5007 Y-13.4390 Z4.4707",
    "G1 X9.3556 Y-13.4347 Z4.7669",
    "G1 X9.2012 Y-13.4303 Z5.0584",
    "G1 X9.0378 Y-13.4260 Z5.3449",
    "G1 X8.8654 Y-13.4216 Z5.6262",
    "G1 X8.6843 Y-13.4173 Z5.9019",
    "G1 X8.4947 Y-13.4129 Z6.1717",
    "G1 X8.2966 Y-13.4085 Z6.4355",
    "G1 X8.0904 Y-13.4042 Z6.6930",
    "G1 X7.8762 Y-13.3998 Z6.9438",
    "G1 X7.6542 Y-13.3955 Z7.1877",
    "G1 X7.4246 Y-13.3911 Z7.4246",
    "G1 X7.1877 Y-13.3868 Z7.6542",
    "G1 X6.9438 Y-13.3824 Z7.8762",
    "G1 X6.6930 Y-13.3781 Z8.0904",
    "G1 X6.4355 Y-13.3737 Z8.2966",
    "G1 X6.1717 Y-13.3694 Z8.4947",
    "G1 X5.9019 Y-13.3650 Z8.6843",
    "G1 X5.6262 Y-13.3606 Z8.8654",
    "G1 X5.3449 Y-13.3563 Z9.0378",
    "G1 X5.0584 Y-13.3519 Z9.2012",
    "G1 X4.7669 Y-13.3476 Z9.3556"
};

void printState() {
    Serial.print("X:");
    Serial.print(state.x, 4);
    Serial.print(" Y:");
    Serial.print(state.y, 4);
    Serial.print(" Z:");
    Serial.print(state.z, 4);
    
    Serial.print(" | Spd(s/s) X:");
    Serial.print(state.speedX, 2);
    Serial.print(" Y:");
    Serial.print(state.speedY, 2);
    Serial.print(" Z:");
    Serial.print(state.speedZ, 2);

    Serial.print(" | Dir: ");
    Serial.print(state.dirX); Serial.print(",");
    Serial.print(state.dirY); Serial.print(",");
    Serial.println(state.dirZ);
    Serial.println("----------------------------------------------------");
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(100);
    }


    Serial.println("=== START TESTU PARSERA G-CODE ===");
    Serial.println("Parametry cewki: D=30mm, 31zw/warstwa\n");
    int numLines = sizeof(testGCodeLines) / sizeof(testGCodeLines[0]);


    for(int i = 0; i < numLines; i++) {
        parse(String(testGCodeLines[i]));
        // Wypisz stan tylko jeśli to nie był sam komentarz (sprawdzamy po zmianie pozycji lub F)
        // Tutaj dla uproszczenia wypisujemy zawsze po przetworzeniu
        printState();
        delay(50); // Małe opóźnienie dla czytelności w terminalu
    }

    Serial.println("=== KONIEC TESTU ===");
    /*Serial.println("--------PROGRAM START-------");
    if(motor1.init())
    {
        Serial.println("Inicjalizacja motora1 zakonczona");
    }

    if(motor2.init())
    {
        Serial.println("Inicjalizacja motora2 zakonczona");
    }

    if(motor1.getProgram() == Stepper::PROGRAM_1)
    {
        Serial.println("Program select motor1: PROGRAM_1");
    }
    else
    {
        Serial.println("Program select motor1: PROGRAM_2");
    }

    if(motor2.getProgram() == Stepper::PROGRAM_1)
    {
        Serial.println("Program select motor2: PROGRAM_1");
    }
    else
    {
        Serial.println("Program select motor2: PROGRAM_2");
    }

    long steps = 200.0f;
    long rewers = (-1) * steps;
    int speed = 100.0f;
    motor1.setEnable(true);
    for(int i = 1; i <= 6; i++)
    {
        motor1.setSpeed(i*speed);
        motor1.moveSteps((i%2 == 0) ? steps : rewers);
        while (motor1.moving())
        {
            delay(1);
        }
        if(!motor1.moving()) Serial.print("Ruch skonczony: "); Serial.println(i);
        delay(3000);
    } */
}

void loop()
{
    
}