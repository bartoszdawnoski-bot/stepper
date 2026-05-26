#include <Arduino.h>
#include <stepper.h>
#include "TMC5160_Adapter.h"
#include "pico/mutex.h"

#define PIO pio0
#define step_pin 1
#define dir_pin 2
#define en_pin 3
#define hold_pin 4

Stepper stepper(PIO, step_pin, dir_pin, en_pin, hold_pin);

//Przy uzywaniu StepSticka sterownika niepotrzebne
//Przez nadpisanie IMotorDriver.h pisze sie obsluge i konfiguracje sterownika
//TMC5160_Adapter adapter(...)

void setup()
{
    //stepper.attachDriver(adpter); jesli stworozny adapter
    stepper.init();
    //przy uzywaniu telemetrii np miedzy rdzeniami trzeba zainicjowac mutex w innych przypadkach biblioteka robi to sama
    //mutex_init(&Stepper::spi_mutex);
    //Stepper::spi_mutex_initialized = true;
    stepper.set_microSteps_mode(Stepper::mode_16);
    stepper.setEnable(true); 
}

void loop()
{
    stepper.addMove(100, 100);
    Stepper::moveSteps();
}