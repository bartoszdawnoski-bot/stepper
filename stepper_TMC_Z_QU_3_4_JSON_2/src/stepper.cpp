#include "stepper.h"
#include <hardware/clocks.h>
#include <hardware/irq.h>
#include "stepper.pio.h" // Wygenerowany plik z asemblera PIO

// Inicjalizacja statycznych tablic instancji
Stepper* Stepper::PIO0_Instance[LENGHT] = {nullptr, nullptr};
Stepper* Stepper::PIO1_Instance[LENGHT] = {nullptr, nullptr};
Stepper* Stepper::PIO2_Instance[LENGHT] = {nullptr, nullptr};

// Maski bitowe do synchronicznego startu silników
uint Stepper::pio0_start_mask = 0;
uint Stepper::pio1_start_mask = 0;
uint Stepper::pio2_start_mask = 0;

Stepper::Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, Program set)
:PIO_instance(pio_instance),
 STEP_PIN(step),
 DIR_PIN(dir),
 ENABLE_PIN(enable),
 HOLD_PIN(hold),
 Program_select(set),
 SM_counter(0),
 SM_speed(0),
 offset_counter(0),
 offset_speed(0),
 isMoving(false),
 Enable(true),
 MS1_PIN(255),
 MS2_PIN(255),
 MS3_PIN(255),
 micro_steps(1),
 position(0)
{   
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
    this->sys_clock = (float)clock_get_hz(clk_sys);
    if(PIO_instance == pio0)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO0_Instance[i] == nullptr)
            {
                PIO0_Instance[i] = this;
                break;
            }
        }

    }
    else if(PIO_instance == pio1)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO1_Instance[i] == nullptr)
            {
                PIO1_Instance[i] = this;
                break;
            }
        } 
    }else if(PIO_instance == pio2)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO2_Instance[i] == nullptr)
            {
                PIO2_Instance[i] = this;
                break;
            }
        } 
    }
    else
    {
        Serial.println("Brak miejsca w wybranym pio");
        return;
    }
    if(Program_select == PROGRAM_1)
    {
        if(PIO_instance == pio0)  this->IRQ = PIO0_IRQ_0;
        else if(PIO_instance == pio1) this->IRQ = PIO1_IRQ_0;
        else if(PIO_instance == pio2) this->IRQ = PIO2_IRQ_0;
        this->irq_num = 0; 
    }
    else if(Program_select == PROGRAM_2)
    {
        if(PIO_instance == pio0)  this->IRQ = PIO0_IRQ_1;
        else if(PIO_instance == pio1) this->IRQ = PIO1_IRQ_1;
        else if(PIO_instance == pio2) this->IRQ = PIO2_IRQ_1;
        this->irq_num = 1; 
    }
}

// Konstruktor główny
Stepper::Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, uint transopt)
:PIO_instance(pio_instance),
 STEP_PIN(step),
 DIR_PIN(dir),
 ENABLE_PIN(enable),
 HOLD_PIN(hold),
 SM_counter(0),
 SM_speed(0),
 offset_counter(0),
 offset_speed(0),
 isMoving(false),
 Enable(false),
 MS1_PIN(255),
 MS2_PIN(255),
 MS3_PIN(255),
 micro_steps(1),
 position(0),
 remaining_steps(0),
 TROPT_PIN(transopt)
{   
    bool program1 = true, program2 = true;

    if(PIO_instance == pio0)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO0_Instance[i] != nullptr)
            {
                if(PIO0_Instance[i]->getProgram() == PROGRAM_1) program1 = false;
                else if(PIO0_Instance[i]->getProgram() == PROGRAM_2) program2 = false;
            }
            if(program1) this->Program_select = PROGRAM_1;
            else if(program2) this->Program_select = PROGRAM_2;
        }
    }
    else if(PIO_instance == pio1)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO1_Instance[i] != nullptr)
            {
                if(PIO1_Instance[i]->getProgram() == PROGRAM_1) program1 = false;
                else if(PIO1_Instance[i]->getProgram() == PROGRAM_2) program2 = false;
            }
            if(program1) this->Program_select = PROGRAM_1;
            else if(program2) this->Program_select = PROGRAM_2;
        }
    }
    else if(PIO_instance == pio2)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO2_Instance[i] != nullptr)
            {
                if(PIO2_Instance[i]->getProgram() == PROGRAM_1) program1 = false;
                else if(PIO2_Instance[i]->getProgram() == PROGRAM_2) program2 = false;
            }
            if(program1) this->Program_select = PROGRAM_1;
            else if(program2) this->Program_select = PROGRAM_2;
        }
    }
    else
    {
        if(Serial) Serial.println("[STEPPER] PIO is full");
        return;
    }

    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH); // Domyślnie wyłącz silnik
    this->sys_clock = (float)clock_get_hz(clk_sys);
    // Rejestracja instancji w odpowiedniej tablicy PIO (dla obsługi przerwań analogicznie dla pio1 i pio2)
    if(PIO_instance == pio0)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO0_Instance[i] == nullptr)
            {
                PIO0_Instance[i] = this;
                break;
            }
        }

    }
    else if(PIO_instance == pio1)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO1_Instance[i] == nullptr)
            {
                PIO1_Instance[i] = this;
                break;
            }
        } 
    }else if(PIO_instance == pio2)
    {
        for(int i = 0; i < LENGHT; i++)
        {
            if(PIO2_Instance[i] == nullptr)
            {
                PIO2_Instance[i] = this;
                break;
            }
        } 
    }
    else
    {
        if(Serial) Serial.println("[STEPPER] Failed to register stepper motor in PIO");
        return;
    }

    // Przypisanie numeru przerwania w zależności od wybranego programu PIO
    // Program 1 używa IRQ 0, Program 2 używa IRQ 1
    if(Program_select == PROGRAM_1)
    {
        if(PIO_instance == pio0)  this->IRQ = PIO0_IRQ_0;
        else if(PIO_instance == pio1) this->IRQ = PIO1_IRQ_0;
        else if(PIO_instance == pio2) this->IRQ = PIO2_IRQ_0;
        this->irq_num = 0; 
    }
    else if(Program_select == PROGRAM_2)
    {
        if(PIO_instance == pio0)  this->IRQ = PIO0_IRQ_1;
        else if(PIO_instance == pio1) this->IRQ = PIO1_IRQ_1;
        else if(PIO_instance == pio2) this->IRQ = PIO2_IRQ_1;
        this->irq_num = 1; 
    }
}

// Inicjalizacja State Machine (SM) w PIO
bool Stepper::init()
{
    // Załaduj program asemblerowy do pamięci instrukcji PIO
    if(Program_select == PROGRAM_1)
    {
        this->offset_counter = pio_add_program(PIO_instance, &step1_counter_program);
        this->offset_speed = pio_add_program(PIO_instance, &step1_speed_program);
    }
    else if(Program_select == PROGRAM_2)
    {
        this->offset_counter = pio_add_program(PIO_instance, &step2_counter_program);
        this->offset_speed = pio_add_program(PIO_instance, &step2_speed_program);
    }
    else
    {
        if(Serial) Serial.println("[STEPPER] No program assigned");
        return false;
    }

    // Zarezerwuj nieużywane State Machines
    SM_counter = pio_claim_unused_sm(PIO_instance, true);
    SM_speed = pio_claim_unused_sm(PIO_instance, true);
    if(SM_counter < 0 || SM_speed < 0) return false;

    // Inicjalizacja maszyn stanów z odpowiednimi pinami
    if(Program_select == PROGRAM_1)
    {
        step1_counter_program_init(PIO_instance, SM_counter, offset_counter, STEP_PIN, HOLD_PIN, 2000000);
        step1_speed_program_init(PIO_instance, SM_speed, offset_speed);
    }
    else if(Program_select == PROGRAM_2)
    {
        step2_counter_program_init(PIO_instance, SM_counter, offset_counter, STEP_PIN, HOLD_PIN, 2000000);
        step2_speed_program_init(PIO_instance, SM_speed, offset_speed);
    }
    else
    {
        if(Serial) Serial.println("[STEPPER] No program assigned");
        return false;
    }

    pinMode(DIR_PIN, OUTPUT);
    digitalWrite(DIR_PIN, LOW);
    if(TROPT_PIN != 255)pinMode(HOLD_PIN, INPUT);
    else pinMode(HOLD_PIN, INPUT_PULLDOWN);
    
    // Konfiguracja przerwań (handler static)
    if(PIO_instance == pio0)
    {
        irq_set_exclusive_handler(IRQ, PIO0_ISR_handler_static);
        irq_set_enabled(IRQ, true);
    }
    else if(PIO_instance == pio1)
    {
        irq_set_exclusive_handler(IRQ, PIO1_ISR_handler_static);
        irq_set_enabled(IRQ, true);
    }
    else if(PIO_instance == pio2)
    {
        irq_set_exclusive_handler(IRQ, PIO2_ISR_handler_static);
        irq_set_enabled(IRQ, true);
    }

    // Włączenie źródła przerwania w PIO
    if(IRQ == PIO0_IRQ_0 || IRQ == PIO1_IRQ_0 || IRQ == PIO2_IRQ_0)
    {
        pio_set_irq0_source_enabled(PIO_instance, pis_interrupt0, true);
    }
    else if(IRQ == PIO0_IRQ_1 || IRQ == PIO1_IRQ_1 || IRQ == PIO2_IRQ_1)
    {
        pio_set_irq1_source_enabled(PIO_instance, pis_interrupt1, true);
    }

    // Maszyny stanów są na razie wyłączone, czekają na start
    pio_sm_set_enabled(PIO_instance, SM_counter, false);
    pio_sm_set_enabled(PIO_instance, SM_speed, false);

    if(Serial) Serial.println("[STEPPER] Stepper motor initialized");
    return true;
}

void Stepper::initTMC(uint16_t cs, float r_sense, uint16_t current_ma)
{
    this->CS_PIN = cs; 
    this->tmc_driver = new TMC5160Stepper(this->CS_PIN, r_sense);
   
   
    this->tmc_driver->begin();
    SPI.beginTransaction(
            SPISettings(
                4000000,
                MSBFIRST,
                SPI_MODE3
            )
        );

    this->tmc_driver->toff(5);
    this->tmc_driver->rms_current(current_ma);

    this->tmc_driver->microsteps(16);
    this->tmc_driver->intpol(true); // Interpolacja do 256
    this->micro_steps = 16;

    this->tmc_driver->en_pwm_mode(true);// stealthChop
    this->tmc_driver->pwm_autoscale(true);

    this->tmc_driver->sgt(10);

    this->use_tmc = true;
    if(Serial) Serial.println("[Stepper] TMC driver initialized");
}

void Stepper::loadToPIO(long steps, float speed) {
    digitalWrite(DIR_PIN, (steps >= 0) ? HIGH : LOW);
    this->futurePosition = (int)steps + this->position;
    
    if(speed <= 0.01f) speed = 0.1f;
    uint32_t delay_cycles = (uint32_t)(2000000.0f / speed);
    if (delay_cycles > 5) delay_cycles -= 10;
    if (delay_cycles < 50) delay_cycles = 50;

    pio_sm_set_enabled(PIO_instance, SM_speed, false);
    pio_sm_set_enabled(PIO_instance, SM_counter, false);
    
    pio_sm_clear_fifos(PIO_instance, SM_speed);
    pio_sm_clear_fifos(PIO_instance, SM_counter);

    pio_sm_restart(PIO_instance, SM_speed);
    pio_sm_restart(PIO_instance, SM_counter);
    
    pio_sm_exec(PIO_instance, SM_speed, pio_encode_jmp(offset_speed));
    pio_sm_exec(PIO_instance, SM_counter, pio_encode_jmp(offset_counter));

    pio_sm_put_blocking(PIO_instance, SM_speed, delay_cycles);
    pio_sm_put_blocking(PIO_instance, SM_counter, (uint32_t)abs(steps));
    
    if(!Enable)
    {
       if(Serial) Serial.println("[STEPPER] Stepper motor is not active"); 
       return; 
    } 
    
    isMoving = true;

    uint start_mask = (1u << this->SM_counter) | (1u << this->SM_speed);
    if(PIO_instance == pio0) pio0_start_mask |= start_mask;
    else if(PIO_instance == pio1) pio1_start_mask |= start_mask;
    else if(PIO_instance == pio2) pio2_start_mask |= start_mask;
}

bool Stepper::addMove(long steps, float speed)
{
    if(!Enable) return false;
    
    // Jeśli bufor pełny zwróć false 
    if(items >= MOTION_BUFFER_SIZE) return false;

    // Dodaj do bufora
    motionBuffer[head].steps = steps;
    motionBuffer[head].speed = speed;
    head = (head + 1) % MOTION_BUFFER_SIZE;
    
    noInterrupts();
    items++;
    interrupts();
    return true;
}

void Stepper::moveSteps() 
{
    
    for(int i=0; i<LENGHT; i++) {
        // PIO 0
        Stepper* s = PIO0_Instance[i];
        if(s != nullptr && !s->moving() && !s->isBufferEmpty()) s->PIO_ISR_Handler(); 
        // PIO 1
        s = PIO1_Instance[i];
        if(s != nullptr && !s->moving() && !s->isBufferEmpty()) s->PIO_ISR_Handler();

        s = PIO2_Instance[i];
        if(s != nullptr && !s->moving() && !s->isBufferEmpty()) s->PIO_ISR_Handler();
    }

    // Odpalenie masek 
    if (pio0_start_mask != 0) pio_set_sm_mask_enabled(pio0, pio0_start_mask, true);
    if (pio1_start_mask != 0) pio_set_sm_mask_enabled(pio1, pio1_start_mask, true);
    if (pio2_start_mask != 0) pio_set_sm_mask_enabled(pio2, pio2_start_mask, true);
    
    pio0_start_mask = 0; pio1_start_mask = 0; pio2_start_mask = 0;
}

// Ustawianie prędkości poprzez dzielnik zegara PIO
void Stepper::setSpeed(float steps_per_second)
{
    // Zabezpieczenie przed dzieleniem przez zero i ujemnymi wartościami
    if(steps_per_second <= 0.01f) return;
    
    uint32_t delay_cycles = (uint32_t)(2000000.0f / steps_per_second);

    if (delay_cycles > 5) delay_cycles -= 10;
    if (delay_cycles < 50) delay_cycles = 50;

    pio_sm_set_enabled(PIO_instance, SM_speed, false);
    // Wyczyść FIFO 
    pio_sm_clear_fifos(PIO_instance, SM_speed);

    // Zresetuj licznik instrukcji do początku programu (offset_speed)
    pio_sm_restart(PIO_instance, SM_speed);
    pio_sm_exec(PIO_instance, SM_speed, pio_encode_jmp(offset_speed));

    // Wyślij nowe opóźnienie do FIFO
    pio_sm_put_blocking(PIO_instance, SM_speed, delay_cycles);

    uint start_mask = (1u << this->SM_speed);
    if(PIO_instance == pio0) pio0_start_mask |= start_mask;
    else if(PIO_instance == pio1) pio1_start_mask |= start_mask;
    else if(PIO_instance == pio2) pio2_start_mask |= start_mask;
    
    // Natychmiastowe włączenie (bo maszyna licznika może już działać)
   if(Serial) 
   {
        Serial.print("Target Speed: "); Serial.print(steps_per_second);
        Serial.print(" Hz | Delay Cycles (2MHz base): "); Serial.println(delay_cycles);
    }
 }

// Ustawienie liczby kroków do wykonania (przygotowanie do ruchu)
void Stepper::setSteps(long double steps)
{
    if(isMoving == true) 
    {
        if(Serial) Serial.println("[STEPPER] Stepper motor is moving");
        return;
    }

    // Ustawienie kierunku
    digitalWrite(DIR_PIN, (steps >= 0) ? HIGH : LOW);
    
    // Obliczenie pozycji docelowej
    this->futurePosition = (int)steps + this->position;
    isMoving = true;
    if(!Enable)
    {
       if(Serial) Serial.println("[STEPPER] Stepper motor is not active"); 
       return; 
    } 

    pio_sm_clear_fifos(PIO_instance, SM_counter);
    pio_sm_put_blocking(PIO_instance, SM_counter, (uint32_t)abs(steps));

    // Przygotuj maskę startową dla synchronicznego uruchomienia
    uint start_mask = (1u << this->SM_counter) | (1u << this->SM_speed);
    
    if(PIO_instance == pio0) pio0_start_mask |= start_mask;
    else if(PIO_instance == pio1) pio1_start_mask |= start_mask;
    else if(PIO_instance == pio2) pio2_start_mask |= start_mask;

}

void Stepper::moveThis(long double steps)
{
    if(isMoving == true) 
    {
        if(Serial) Serial.println("[STEPPER] Stepper motor is moving");
        return;
    }
    digitalWrite(DIR_PIN, (steps >= 0) ? HIGH : LOW);
    
    this->futurePosition = (int)steps + this->position;
    isMoving = true;
    if(!Enable)
    {
       if(Serial) Serial.println("[STEPPER] Stepper motor is not active"); 
       return; 
    } 
    else
    {
        digitalWrite(this->ENABLE_PIN, LOW);
    }

    pio_sm_set_enabled(PIO_instance, SM_counter, true);
    pio_sm_set_enabled(PIO_instance, SM_speed, true);

    pio_sm_clear_fifos(PIO_instance, SM_counter);
    pio_sm_put_blocking(PIO_instance, SM_counter, (uint32_t)abs(steps));
    
}

void Stepper::PIO0_ISR_handler_static()
{
    for(int i = 0; i < LENGHT; i++)
    {
        if(PIO0_Instance[i] != nullptr && pio_interrupt_get(pio0, PIO0_Instance[i]->irq_num))
        {
            PIO0_Instance[i]->PIO_ISR_Handler();
        }
    }
}

void Stepper::PIO1_ISR_handler_static()
{
    for(int i = 0; i < LENGHT; i++)
    {
        if(PIO1_Instance[i] != nullptr && pio_interrupt_get(pio1, PIO1_Instance[i]->irq_num))
        {
            PIO1_Instance[i]->PIO_ISR_Handler();
        }
    }
}

void Stepper::PIO2_ISR_handler_static()
{
    for(int i = 0; i < LENGHT; i++)
    {
        if(PIO2_Instance[i] != nullptr && pio_interrupt_get(pio2, PIO2_Instance[i]->irq_num))
        {
            PIO2_Instance[i]->PIO_ISR_Handler();
        }
    }
}
// Handler przerwania - wywoływany przez PIO gdy licznik kroków dojdzie do 0 albo pojawi sie przerwanie
void Stepper::PIO_ISR_Handler()
{
    if(pio_interrupt_get(this->PIO_instance, this->irq_num)) 
    {
        pio_interrupt_clear(PIO_instance, irq_num);
    }

    if(digitalRead(this->HOLD_PIN) == HIGH) 
    {
        this->setZero(); 
        this->items = 0;
        this->head = 0;
        this->tail = 0;
        
        // Wyłączamy flagę ruchu i maszyny stanów
        this->isMoving = false;
        pio_sm_set_enabled(PIO_instance, SM_counter, false);
        pio_sm_set_enabled(PIO_instance, SM_speed, false);
        digitalWrite(this->ENABLE_PIN, HIGH); 
        return; 
    }

    if(isMoving) 
    {
        this->position = this->futurePosition;
    }

    if(items > 0)
    {
        long next_steps = motionBuffer[tail].steps;
        float next_speed = motionBuffer[tail].speed;
        
        tail = (tail + 1) % MOTION_BUFFER_SIZE;
        noInterrupts();
        items--;
        interrupts();

        loadToPIO(next_steps, next_speed);

        uint mask = 0;
        if(PIO_instance == pio0) { mask = pio0_start_mask; pio0_start_mask = 0; pio_set_sm_mask_enabled(pio0, mask, true); }
        else if(PIO_instance == pio1) { mask = pio1_start_mask; pio1_start_mask = 0; pio_set_sm_mask_enabled(pio1, mask, true); }
        else if(PIO_instance == pio2) { mask = pio2_start_mask; pio2_start_mask = 0; pio_set_sm_mask_enabled(pio2, mask, true); }
    } 
    else
    {
        isMoving = false;
        pio_sm_set_enabled(PIO_instance, SM_counter, false);
        pio_sm_set_enabled(PIO_instance, SM_speed, false);
    }
}
void Stepper::zero()
{
    long pos = (-1)*this->position;
    this->moveThis(pos); 
}
void Stepper::setZero()
{
    this->position = 0;
    this->remaining_steps = 0;
    this->futurePosition = 0;
    this->isMoving = false;
}
bool Stepper::moving()
{
    return this->isMoving;
}
void Stepper::set_microSteps_mode(Microsteps mode)
{
    if(use_tmc)
    {
        switch(mode)
        {
            case mode_1: this->micro_steps = 1; break;
            case mode_2: this->micro_steps = 2; break;
            case mode_4: this->micro_steps = 4; break;
            case mode_8: this->micro_steps = 8; break;
            case mode_16: this->micro_steps = 16; break;
            case mode_32: this->micro_steps = 32;  break;
            case mode_64: this->micro_steps = 64;  break;
            case mode_128: this->micro_steps = 128; break;
            case mode_256: this->micro_steps = 256; break;
            default: this->micro_steps = 16; break;
        }

        this->tmc_driver->microsteps(this->micro_steps);

        //przy natywnych mikrokrokach interpolacja nie działa więc trzeba ją wyłączyć
        if(this->micro_steps == 256) this->tmc_driver->intpol(false);
        else this->tmc_driver->intpol(true);

        return;
    }
    
    if(MS1_PIN == 255 && MS2_PIN == 255 && MS3_PIN == 255) return;

    switch (mode)
    {
    case mode_1: 
        digitalWrite(MS1_PIN, LOW); 
        digitalWrite(MS2_PIN, LOW); 
        digitalWrite(MS3_PIN, LOW); 
        this->micro_steps = 1;
    break;
    case mode_2: 
        digitalWrite(this->MS1_PIN, HIGH); 
        digitalWrite(this->MS2_PIN, LOW); 
        digitalWrite(this->MS3_PIN, LOW); 
        this->micro_steps = 2;
    break;
    case mode_4: 
        digitalWrite(this->MS1_PIN, LOW); 
        digitalWrite(this->MS2_PIN, HIGH); 
        digitalWrite(this->MS3_PIN, LOW); 
        this->micro_steps = 4;
    break;
    case mode_8: 
        digitalWrite(this->MS1_PIN, HIGH); 
        digitalWrite(this->MS2_PIN, HIGH); 
        digitalWrite(this->MS3_PIN, LOW); 
        this->micro_steps = 8;
    break;
    case mode_16: 
        digitalWrite(this->MS1_PIN, HIGH); 
        digitalWrite(this->MS2_PIN, HIGH); 
        digitalWrite(this->MS3_PIN, HIGH); 
        this->micro_steps = 16;
    break;
    default:
        digitalWrite(this->MS1_PIN, LOW); 
        digitalWrite(this->MS2_PIN, LOW); 
        digitalWrite(this->MS3_PIN, LOW); 
        this->micro_steps = 1;
    }
}

uint Stepper::get_microsteps()
{
    return this->micro_steps;
}

void Stepper::set_microSteps_pins(uint ms1, uint ms2, uint ms3)
{
    this->MS1_PIN = ms1;
    this->MS2_PIN = ms2;
    this->MS3_PIN = ms3;

    pinMode(MS1_PIN, OUTPUT);
    pinMode(MS2_PIN, OUTPUT);
    pinMode(MS3_PIN, OUTPUT);
}

Stepper::Program Stepper::getProgram()
{
    return this->Program_select;
}

bool Stepper::setEnable(bool set)
{
    if(set)
    {
        digitalWrite(this->ENABLE_PIN, LOW);
        this->Enable = true;
        return true;
    }
    digitalWrite(this->ENABLE_PIN, HIGH);
    this->Enable = false;
    return false; 
}

int Stepper::getPosition()
{
    return this->position;
}

uint16_t Stepper::get_load()
{
    if(!use_tmc) return 0;
    // sg_result zwraca wartość obciążenia.
    return this->tmc_driver->sg_result();
}

uint32_t Stepper::get_status()
{
    if(!use_tmc) return 0;
    return this->tmc_driver->DRV_STATUS();
}

bool Stepper::is_overheated()
{
    if(!use_tmc) return false;
    // Sprawdza flagę 'ot' w rejestrze statusu
    return this->tmc_driver->ot();
}

bool Stepper::get_tmc()
{
    return this->use_tmc;
}

void Stepper::e_stop()
{
    digitalWrite(this->ENABLE_PIN, HIGH);
    pio_sm_set_enabled(PIO_instance, SM_counter, false);
    pio_sm_set_enabled(PIO_instance, SM_speed, false);
    this->isMoving = false;
    this->items = 0;
    this->head = 0;
    this->tail = 0;

    pio_sm_restart(PIO_instance, SM_speed);
    pio_sm_exec(PIO_instance, SM_speed, pio_encode_jmp(offset_speed));
    pio_sm_clear_fifos(PIO_instance, SM_speed);

    pio_sm_restart(PIO_instance, SM_counter);
    pio_sm_exec(PIO_instance, SM_counter, pio_encode_jmp(offset_counter));
    pio_sm_clear_fifos(PIO_instance, SM_counter);

    hw_clear_bits(&PIO_instance->irq, 0xFF);
}

bool Stepper::isBufferFull()
{
    return this->items >= MOTION_BUFFER_SIZE;
}

bool Stepper::isBufferEmpty()
{
    return this->items == 0;
}