#include <Arduino.h>
#include "stepper.h"

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
void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(100);
    }

    Serial.println("--------PROGRAM START-------");
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

    long steps = 150.0f;
    long rewers = (-1) * steps;
    int speed = 100.0f;
    for(int i = 1; i <= 6; i++)
    {
        motor1.setSpeed(i*speed);
        motor1.moveSteps((i%2 == 0) ? steps : rewers);
        delay(100);
        motor1.setSpeed((i*2*speed <= 600) ? (i*2*speed) : i*speed); 
        while (motor1.moving())
        {
            delay(1);
        }
        if(!motor1.moving()) Serial.print("Ruch skonczony: " + i);
        delay(3000);
    } 
}

void loop()
{
    
}