#include "gcode.h"

static float get_current_mA_from_grams(float target_grams) {
    float target_mN = target_grams * 9.80665f; // Konwersja gramów na miliNiutony
    
    // Równanie kwadratowe: 0.00048*I^2 + 0.22*I + (185 - target_mN) = 0
    float a = 0.00048f;
    float b = 0.22f;
    float c = 185.0f - target_mN;
    
    float delta = (b * b) - (4.0f * a * c);
    
    float target_mA = 100.0f; // Domyślnie bezpieczne 100 mA
    if (delta >= 0) {
        target_mA = (-b + sqrtf(delta)) / (2.0f * a);
    }

    if (target_mA < 100.0f) target_mA = 100.0f;
    
    return target_mA;
}

// Konstruktory dla 2 silników
GCode::GCode(Stepper* stX, Stepper* stY):
stepperX(stX),
stepperY(stY),
current_pos_x(0.0f), 
current_pos_y(0.0f)
{
        if(stepperX->get_microsteps() != stepperY->get_microsteps())
    {
        web_log("[GCODE] Błąd: Silniki X i Y mają różne ustawienia mikrokroków. Upewnij się, że oba silniki mają takie same ustawienia.");
        this->actual_microsteps = 0;
        return;
    }

    this->actual_microsteps = stepperX->get_microsteps();
}

GCode::GCode(Stepper* stX, Stepper* stY, Stepper* stZ):
stepperX(stX),
stepperY(stY),
stepperZ(stZ),
current_pos_x(0.0f), 
current_pos_y(0.0f)
{
    if(stepperX->get_microsteps() != stepperY->get_microsteps())
    {
        web_log("[GCODE] Błąd: Silniki X i Y mają różne ustawienia mikrokroków. Upewnij się, że oba silniki mają takie same ustawienia.");
        this->actual_microsteps = 0;
        return;
    }

    this->actual_microsteps = stepperX->get_microsteps();

}

// Konwersja milimetrów na kroki
long GCode::steps_from_MM(float mm, float stepsPerMM)
{
    int microsteps = (stepperX) ? stepperX->get_microsteps() : 1;
    return (long)(mm * stepsPerMM * microsteps);
}

// Konwersja obrotów na kroki
long GCode::steps_from_rotation(float rotation, float stepsPerRotation)
{
    int microsteps = (stepperY) ? stepperY->get_microsteps() : 1;
    return (long)(rotation * stepsPerRotation * microsteps);
}

// Główna funkcja wykonawcza parsująca i uruchamiająca silniki
void GCode::execute_parse()
{
    if(this->e_stop) return;
    // Jeśli linia pusta, wyjdź
    if(parser.NoWords()) return;
    if(parser.HasWord('M'))
    {
        int num = (int)parser.GetWordValue('M');
        if(num == 18)
        {
            web_log("[GCODE] Silniki wyłączone");
            stepperX->setEnable(false);
            stepperY->setEnable(false);
            if(stepperZ != nullptr) stepperZ->setEnable(false);
        }
        if(num == 17)
        {
            web_log("[GCODE] Silniki włączone");
            stepperX->setEnable(true);
            stepperY->setEnable(true);
            if(stepperZ != nullptr) stepperZ->setEnable(true);
        }
        if(num == 994)
        {
            if(parser.HasWord('X'))
            {
                float val = parser.GetWordValue('X');
                if(val > stepperX->get_max_current()) web_log("[GCODE] Prąd wózka przekracza wcześniej ustawiony maksymalny limit");
                stepperX->set_max_current(val);
                web_log("[GCODE] Maksymalny prąd wózka ustawiony na: " + String(val) + " mA");
            }

            if(parser.HasWord('Y'))
            {
                float val = parser.GetWordValue('Y');
                if(val > stepperY->get_max_current()) web_log("[GCODE] Prąd wrzeciona przekracza wcześniej ustawiony maksymalny limit");
                stepperY->set_max_current(val);
                web_log("[GCODE] Maksymalny prąd wrzeciona ustawiony na: " + String(val) + " mA");
            }
        }
    }
    // Obsługa komend G
    if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        if(num == 90) { relative_mode = false; web_log("[GCODE] tryb realtywny wyłączony"); return; }
        if(num == 91) { relative_mode = true;   web_log("[GCODE] tryb relatywny włączony"); return;}
        if(num == 92)
        {
            move_complete();
            bool any_axis = false;

            if(parser.HasWord('X'))
            {
                float val = current_pos_x;
                current_pos_x = 0.0f;
                last_stepX = 0;
                this->stepperX->setZero();
                web_log("[GCODE] Ustawiono logiczne 0 w X = " + String(val));
                any_axis = true;
            }

            if(parser.HasWord('Y'))
            {
                float val = current_pos_y;
                current_pos_y = 0.0f;
                last_stepY = 0; 
                this->stepperY->setZero();
                web_log("[GCODE] Ustawiono logiczne 0 w Y = " + String(val));
                any_axis = true;
            }

            if(!any_axis)
            {
                last_stepX = 0; 
                last_stepY = 0; 
                this->stepperX->setZero();
                this->stepperY->setZero();
                web_log("[GCODE] Wyzerowano obie osie (0, 0) w punkcie (" + String(current_pos_x) + ", " + String(current_pos_y) + ")");
                current_pos_x = 0.0f; 
                current_pos_y = 0.0f;
            }
            return;
        }
        if(num == 28)
        {
            if(num == 28)
            {
                move_complete();
                if(Serial) Serial.println("[GCODE] Homing procedure started");
                web_log("[GCODE] Start procedury");

                float speedX_steps = 5.0f * factor.steps_perMM_x * stepperX->get_microsteps();            
                digitalWrite(TRANSOPT_PIN_A, HIGH);
                stepperX->addMove(-1000000.0 * stepperX->get_microsteps(), speedX_steps);
                Stepper::moveSteps();
                move_complete();
                digitalWrite(TRANSOPT_PIN_A, LOW);

                float speedY_steps = 1.0f * factor.steps_per_rotation_c * stepperY->get_microsteps();
                digitalWrite(TRANSOPT_PIN_B, HIGH);
                stepperY->addMove(-1000000.0 * stepperY->get_microsteps(), speedY_steps);
                Stepper::moveSteps();
                move_complete();
                digitalWrite(TRANSOPT_PIN_B, LOW);

                last_stepX = 0;
                last_stepY = 0;
                current_pos_x = 0;
                current_pos_y = 0;
                if(Serial) Serial.println("[GCODE] Homing Done");
                web_log("[GCODE] Punkt zero ustawiony");
                return;
            }
        }
        if(num == 30)
        {
            move_complete(); 
            if(Serial) Serial.println("[GCODE] Returning to software home (0,0)");
            web_log("[GCODE] Powrot do zera");

            float m_x = (stepperX) ? stepperX->get_microsteps() : 1;
            float m_y = (stepperY) ? stepperY->get_microsteps() : 1;

            float speedX_steps = (factor.v_max_x * 0.5f) * factor.steps_perMM_x * m_x;
            float speedY_steps = (factor.v_max_y * 0.5f) * factor.steps_per_rotation_c * m_y;

            stepperX->zero(speedX_steps);        
            stepperY->zero(speedY_steps);
            
            Stepper::moveSteps();

            move_complete();

            current_pos_x = 0;
            current_pos_y = 0;
            last_stepX = 0;
            last_stepY = 0;
            
            return;
        }
        if(num == 1 || num == 0)
        {
            //Aktualizacja prędkości 
            if (parser.HasWord('F'))
            {
                float val = parser.GetWordValue('F');
                if (val > 0.0f) current_feedrate_mm_min = val;
            }

            // Obliczanie celu 
            float next_pos_x = current_pos_x;
            float next_pos_y = current_pos_y;

            if(parser.HasWord('X'))
            {
               float val = parser.GetWordValue('X');
                if (relative_mode) next_pos_x += val; 
                else next_pos_x = val;
            }

            if (parser.HasWord('Y'))
            {
                float val = parser.GetWordValue('Y');
                if (relative_mode) next_pos_y += val; 
                else next_pos_y = val;
            }

            float start_z_ma = (stepperZ != nullptr) ? stepperZ->get_actuall_current() : 100.0f;
            float target_z_ma = start_z_ma;

            if(parser.HasWord('Z'))
            {
                float val = parser.GetWordValue('Z'); 
                target_z_ma = get_current_mA_from_grams(val); 
                
                if (target_z_ma > factor.max_current_Z) target_z_ma = factor.max_current_Z;

                if (stepperZ != nullptr && !stepperZ->isEnabled()) {
                    stepperZ->setEnable(true); 
                }
            }

            float delta_mm_x = next_pos_x - current_pos_x;
            float delta_unit_y = next_pos_y - current_pos_y;

            float total_dist = sqrtf(delta_mm_x*delta_mm_x + delta_unit_y*delta_unit_y);
            long targetStepX = steps_from_MM(next_pos_x, factor.steps_perMM_x);
            long targetStepY = steps_from_rotation(next_pos_y, factor.steps_per_rotation_c);

            long diffX = targetStepX - last_stepX;
            long diffY = targetStepY - last_stepY;

            if (diffX == 0 && diffY == 0) 
            {
                if (start_z_ma != target_z_ma && stepperZ != nullptr) {
                    stepperZ->set_current((uint16_t)target_z_ma);
                }
                current_pos_x = next_pos_x;
                current_pos_y = next_pos_y;
                return; 
            }

           float vX = 0, vY = 0;
            if (total_dist > 0.00001f)
            {
                float speed_unit_sec = current_feedrate_mm_min / 60.0f;
                
                int micro_x = (stepperX) ? stepperX->get_microsteps() : 1;
                int micro_y = (stepperY) ? stepperY->get_microsteps() : 1;

                // Prędkość w krokach/s = (Prędkość w mm/s) * (Udział osi) * (Kroki/mm)
                vX = speed_unit_sec * (fabs(delta_mm_x) / total_dist) * factor.steps_perMM_x * micro_x;
                vY = speed_unit_sec * (fabs(delta_unit_y) / total_dist) * factor.steps_per_rotation_c * micro_y;

                float max_step_rate_x = factor.v_max_x * factor.steps_perMM_x * micro_x;
                float max_step_rate_y = factor.v_max_y * factor.steps_per_rotation_c * micro_y;

                // Limity prędkości 
                if(vX > max_step_rate_x) { float r = max_step_rate_x / vX; vX *= r; vY *= r; }
                if(vY > max_step_rate_y) { float r = max_step_rate_y / vY; vX *= r; vY *= r; }
            }

            long max_steps = max(abs(diffX), abs(diffY));

            bool is_starting_from_zero = stepperX->isBufferEmpty() && !stepperX->moving() && stepperY->isBufferEmpty() && !stepperY->moving();
            if (is_starting_from_zero)
            {
                steps_in_acceleration = 0;
                current_run_multiplier = 0.1f;
            }

           long steps_sent = 0;
            long sentX = 0, sentY = 0;

            // Zapamiętujemy punkty startowe przed rozpoczęciem ruchu
            long start_stepX = last_stepX;
            long start_stepY = last_stepY;
            float start_pos_x = current_pos_x;
            float start_pos_y = current_pos_y;

            while (steps_sent < max_steps)
            {
                if(this->e_stop) return; 

                long active_chunk_size = (steps_in_acceleration < (RAMP_STEPS * actual_microsteps) || max_steps - steps_sent < (RAMP_STEPS * actual_microsteps) ) ? (40 * actual_microsteps) : CHUNK_SIZE * actual_microsteps;                long current_chunk = min(active_chunk_size, max_steps - steps_sent);
                long steps_remaining = max_steps - steps_sent;

                steps_in_acceleration += current_chunk;

                long current_ramp_steps = min(RAMP_STEPS*actual_microsteps, max_steps / 2); 
                if (current_ramp_steps < 10) current_ramp_steps = 10; // Zabezpieczenie przed zerem

                if (steps_in_acceleration < current_ramp_steps) 
                {
                    float ramp_progress = (float)steps_in_acceleration / current_ramp_steps;
                    // Płynne skalowanie bez sztywnych ułamków
                    current_run_multiplier = V_MIN + ((V_MAX - V_MIN) * ramp_progress); 
                }
                else if (!this->next_line_available && steps_remaining < current_ramp_steps) 
                {
                    float ramp_progress = (float)steps_remaining / current_ramp_steps;
                    current_run_multiplier = V_MIN + ((V_MAX - V_MIN) * ramp_progress);
                }
                else 
                {
                    current_run_multiplier = V_MAX;
                }

                if(current_run_multiplier > V_MAX) current_run_multiplier = V_MAX;
                if(current_run_multiplier < V_MIN) current_run_multiplier = V_MIN;

                steps_sent += current_chunk;
                
                double progress = (double)steps_sent / max_steps;
                long target_X = (long)(diffX * progress);
                long target_Y = (long)(diffY * progress);

                long stepX_chunk = target_X - sentX;
                long stepY_chunk = target_Y - sentY;

                while((stepX_chunk != 0 && stepperX->isBufferFull()) || 
                      (stepY_chunk != 0 && stepperY->isBufferFull()))
                {
                    if(this->e_stop) return; 
                    Stepper::moveSteps();
                    watchdog_update();
                    delay(1);
                }

                // Wypychanie kroków z przeliczonym mnożnikiem
                if (stepX_chunk != 0) stepperX->addMove(stepX_chunk, vX, true, current_run_multiplier);
                if (stepY_chunk != 0) stepperY->addMove(stepY_chunk, vY, true, current_run_multiplier);
            
                if (start_z_ma != target_z_ma && stepperZ != nullptr) {
                    uint16_t interp_ma = (uint16_t)(start_z_ma + (target_z_ma - start_z_ma) * progress);
                    
                    if (interp_ma < 100) interp_ma = 100;
                    if (interp_ma > factor.max_current_Z) interp_ma = (uint16_t)factor.max_current_Z;
                    
                    stepperZ->set_current(interp_ma);
                }

                Stepper::moveSteps();

                // Aktualizujemy wysłane kroki dopiero po faktycznym wrzuceniu ich do bufora
                sentX = target_X;
                sentY = target_Y;

                // Proporcjonalna aktualizacja pozycji po każdym wysłanym chunku
                last_stepX = start_stepX + sentX;
                last_stepY = start_stepY + sentY;
                current_pos_x = start_pos_x + (delta_mm_x * (float)progress);
                current_pos_y = start_pos_y + (delta_unit_y * (float)progress);
            }

            // Po pełnym, nieprzerwanym wykonaniu pętli przypisujemy dokładne wartości końcowe
            last_stepX = targetStepX;
            last_stepY = targetStepY;
            current_pos_x = next_pos_x;
            current_pos_y = next_pos_y;
        }
    }
    else if(!parser.HasWord('M') && !parser.HasWord('G'))
        web_log("[GCODE] Zignorowano nierozpoznaną komendę");
}

// Funkcja publiczna do przetwarzania surowego ciągu znaków
void GCode::processLine(const String& line, bool has_next)
{
    next_line_available = has_next;

    // Ignoruj puste linie i komentarze
    if(line.length() == 0 || line[0] == ';') return;

    char buffer[128];
    line.toCharArray(buffer, 128);

    // Parsuj linię i wykonaj
    parser.ParseLine(buffer);
    this->execute_parse();
}

// Funkcja blokująca czeka aż wszystkie silniki zakończą pracę
void GCode::move_complete()
{
    while((stepperX->moving() || !stepperX->isBufferEmpty()) || (stepperY->moving() || !stepperY->isBufferEmpty()))
    {
        if(this->e_stop) return;
        tight_loop_contents();
        watchdog_update();
    }

}

void GCode::update_settings(MachineSettings &cfg) {
    factor.v_max_x = cfg.sx;
    factor.v_max_y = cfg.sy;
    factor.steps_perMM_x = cfg.st_mm;
    factor.steps_per_rotation_c = cfg.st_rot;
    factor.max_current_Z = cfg.maxz;

    set_chunk_size(cfg.chunk_size);
    set_ramp_steps(cfg.ramp_steps);
    set_v_min(cfg.v_min);

    this->actual_microsteps = cfg.micro_steps;
    
    Stepper::Microsteps resolved_mode;
    switch(cfg.micro_steps) {
        case 1: resolved_mode = Stepper::mode_1; break;
        case 2: resolved_mode = Stepper::mode_2; break;
        case 4: resolved_mode = Stepper::mode_4; break;
        case 8: resolved_mode = Stepper::mode_8; break;
        case 16: resolved_mode = Stepper::mode_16; break;
        case 32: resolved_mode = Stepper::mode_32; break;
        case 64: resolved_mode = Stepper::mode_64; break;
        case 128: resolved_mode = Stepper::mode_128; break;
        case 256: resolved_mode = Stepper::mode_256; break;
        default: resolved_mode = Stepper::mode_16; this->actual_microsteps = 16; break;
    }

    stepperX->set_microSteps_mode(resolved_mode);
    stepperY->set_microSteps_mode(resolved_mode);

    if(cfg.curr_x >= stepperX->get_max_current()) factor.curr_x = stepperX->get_max_current();
    else factor.curr_x = cfg.curr_x;

    if(cfg.curr_y >= stepperY->get_max_current()) factor.curr_y = stepperY->get_max_current();
    else factor.curr_y = cfg.curr_y;

    if(stepperX->get_actuall_current() != factor.curr_x)
    {
        this->stepperX->set_current(factor.curr_x);
        web_log(String("[LITTLEFS] Ustawiono prąd wózka: ") + factor.curr_x + String(" mA"));
    }

    if(stepperY->get_actuall_current() != factor.curr_y)
    {
        this->stepperY->set_current(factor.curr_y);
        web_log(String("[LITTLEFS] Ustawiono prąd wrzeciona: ") + factor.curr_y + String(" mA"));
    }

    if(stepperZ != nullptr && stepperZ->get_max_current() != cfg.maxz)
    {
        this->stepperZ->set_max_current(cfg.maxz);
        web_log(String("[LITTLEFS] Ustawiono maxymalny prąd hamulca: ") + cfg.maxz + String(" mA"));
    }
}
bool GCode::is_em_stopped()
{
    return this->e_stop;
}

void GCode::em_stopp() 
{ 
    this->e_stop = true; 
}

void GCode::em_stopp_f() 
{ 
    this->e_stop = false; 
}

float GCode::get_current_Z() 
{
    return this->factor.max_current_Z;
}

float GCode::get_steps_perMM_x()
{
    return factor.steps_perMM_x;
}

float GCode::get_steps_per_rotation_c()
{
    return factor.steps_per_rotation_c;
}

float GCode::get_current_x()
{
    return this->stepperX->get_actuall_current();
}

float GCode::get_current_y()
{
    return this->stepperY->get_actuall_current();
}

uint GCode::get_microsteps()
{
    return this->actual_microsteps;
}

bool GCode::is_relative_mode()
{
    return this->relative_mode;
}

void GCode::set_chunk_size(long chunk_size)
{
    if(chunk_size < 1) chunk_size = 1;
    this->CHUNK_SIZE = chunk_size;
}

void GCode::set_ramp_steps(long ramp_steps)
{
    if(ramp_steps < 1) ramp_steps = 1;
    this->RAMP_STEPS = ramp_steps;
}

void GCode::set_v_min(float v_min)
{
    if(v_min < 0.01f) v_min = 0.01f;
    if(v_min > 1.0f) v_min = 1.0f;
    this->V_MIN = v_min;
}

void GCode::set_v_max(float v_max)
{
    if(v_max < 0.01f) v_max = 0.01f;
    if(v_max > 1.0f) v_max = 1.0f;
    this->V_MAX = v_max;
}
