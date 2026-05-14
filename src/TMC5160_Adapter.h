#pragma once
#include "IMotorDriver.h"
#include <TMCStepper.h>
#include <SPI.h>

class TMC5160_Adapter : public IMotorDriver {
private:
    TMC5160Stepper* driver = nullptr; // Wskaźnik, pusty na starcie
    uint16_t cs_pin;
    float r_sense;
    uint16_t actual_current;

public:
    TMC5160_Adapter(uint16_t cs, float rs) : 
    cs_pin(cs), 
    r_sense(rs), 
    actual_current(0) 
    {
    }

    bool init(uint16_t current_ma) override 
    {
        if (driver == nullptr) {
            driver = new TMC5160Stepper(cs_pin, r_sense);
        }
        
        driver->begin();
        driver->toff(5);
        driver->rms_current(current_ma);
        this->actual_current = current_ma;
        driver->microsteps(16);
        driver->en_pwm_mode(true); // stealthChop
        driver->pwm_autoscale(true);
        driver->sgt(-40); // Domyślna czułość StallGuard
        
        return true;
    }

    void set_current(uint16_t ma) override 
    { 
        if(driver) {
            driver->rms_current(ma); 
            this->actual_current = ma;
        }
    }
    
    void set_microsteps(uint16_t ms) override
    { 
        if(driver) {
            driver->microsteps(ms); 
            driver->intpol(ms != 256);
        }
    }
    
    float get_actual_current() override { return (float)this->actual_current; }
    
    uint16_t get_load() override { return driver ? driver->sg_result() : 0; }
    uint32_t get_status() override { return driver ? driver->DRV_STATUS() : 0; }
    bool is_overheated() override { return driver ? driver->ot() : false; }
};