#define STEPPER_H
#ifdef STEPPER_H

#include <Arduino.h>
#include <hardware/pio.h>

#define LENGHT 2

class Stepper
{
public:
    enum Program
    {
        PROGRAM_1,
        PROGRAM_2
    };

    enum Microsteps
    {
        mode_1,
        mode_2,
        mode_4,
        mode_8,
        mode_16
    };

private:
    PIO PIO_instance;
    uint STEP_PIN, DIR_PIN, ENABLE_PIN, HOLD_PIN, MS1_PIN, MS2_PIN, MS3_PIN;
    Program Program_select;
    irq_num_rp2350 IRQ;
    uint irq_num, micro_steps;
    uint SM_counter, SM_speed;
    uint offset_counter, offset_speed;

    static Stepper* PIO0_Instance[LENGHT];
    static Stepper* PIO1_Instance[LENGHT]; 
    static Stepper* PIO2_Instance[LENGHT];
    static uint pio0_start_mask;
    static uint pio1_start_mask;
    static uint pio2_start_mask;

    float sys_clock;
    float steps_per_second;
    int position, remaining_steps, futurePosition;
    volatile bool isMoving;
    bool Enable;

    void PIO_ISR_Handler();

public:
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, Program set);
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold);
    static void moveSteps();
    bool init();
    void setSpeed(float steps_per_second);
    void setSteps(long double steps);
    void moveThis(long double steps);
    static void PIO0_ISR_handler_static();
    static void PIO1_ISR_handler_static();
    static void PIO2_ISR_handler_static();
    bool moving();
    void set_microSteps_pins(uint ms1, uint ms2, uint ms3);
    void set_microSteps_mode(Microsteps mode);
    Program getProgram();
    bool setEnable(bool set);
    int getPosition();
    void zero();
    void setZero();
};

#endif 