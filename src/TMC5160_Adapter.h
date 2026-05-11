#pragma once
#include "IMotorDriver.h"
#include <TMCStepper.h>
#include <SPI.h>

class TMC5160_Adapter : public IMotorDriver {
private:
    TMC5160Stepper* driver;
    uint16_t cs_pin;
    float r_sense;
    uint16_t actual_current;

public:
    TMC5160_Adapter(uint16_t cs, float rs) : 
    cs_pin(cs), 
    r_sense(rs), 
    actual_current(0) 
    {
        driver = new TMC5160Stepper(cs_pin, r_sense);
    }

    bool init(uint16_t current_ma) override 
    {
        driver->begin();
        
        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE3));
        driver->toff(5);
        driver->rms_current(current_ma);
        this->actual_current = current_ma;
        driver->microsteps(16);
        driver->en_pwm_mode(true); // stealthChop
        driver->pwm_autoscale(true);
        driver->sgt(0); // Domyślna czułość do kalibracji
        SPI.endTransaction();
        
        return true;
    }

    void set_current(uint16_t ma) override 
    { 
        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE3));
        driver->rms_current(ma); 
        this->actual_current = ma;
        SPI.endTransaction();
    }
    
    void set_microsteps(uint16_t ms) override
     { 
        driver->microsteps(ms); 
        driver->intpol(ms != 256);
    }
    
    float get_actual_current() override { return (float)this->actual_current; }
    uint16_t get_load() override { return driver->sg_result(); }
    uint32_t get_status() override { return driver->DRV_STATUS(); }
    bool is_overheated() override { return driver->ot(); }
};