#pragma once
#include <Arduino.h>

class IMotorDriver {
public:
    virtual ~IMotorDriver() = default;

    virtual bool init(uint16_t current_ma) = 0;
    virtual void set_current(uint16_t ma) = 0;
    virtual void set_microsteps(uint16_t ms) = 0;
    virtual float get_actual_current() = 0;

    virtual uint16_t get_load() = 0; 
    virtual uint32_t get_status() = 0;
    virtual bool is_overheated() = 0;
};