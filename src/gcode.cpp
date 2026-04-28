#include "gcode.h"


// Konstruktory dla 2 silników
GCode::GCode(Stepper* stX, Stepper* stY):
stepperX(stX),
stepperY(stY),
current_pos_x(0.0f), 
current_pos_y(0.0f)
{}

GCode::GCode(Stepper* stX, Stepper* stY, Stepper* stZ):
stepperX(stX),
stepperY(stY),
stepperZ(stZ),
current_pos_x(0.0f), 
current_pos_y(0.0f)
{}

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
            stepperX->setEnable(false);
            stepperY->setEnable(false);
            stepperZ->setEnable(false);
        }
        if(num == 84)
        {
            stepperX->setEnable(true);
            stepperY->setEnable(true);
            stepperZ->setEnable(true);
        }
    }
    // Obsługa komend G
    if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        if(num == 90) { relative_mode = false; return; }
        if(num == 91) { relative_mode = true;  return;}
        if(num == 29)
        {
            last_stepX = 0; 
            last_stepY = 0; 
            current_pos_x = 0.0f; 
            current_pos_y = 0.0f; 
            return;
        }
        if(num == 28)
        {
            move_complete();
            if(Serial) Serial.println("[GCODE] Homing procedure started");

            digitalWrite(TRANSOPT_PIN_A, HIGH);
            stepperX->addMove(-1000000.0, factor.v_max_x * 0.5f);
            Stepper::moveSteps();
            move_complete();
            digitalWrite(TRANSOPT_PIN_A, LOW);

            digitalWrite(TRANSOPT_PIN_B, HIGH);
            stepperY->addMove(-1000000.0, factor.v_max_y * 0.5f);
            Stepper::moveSteps();
            move_complete();
            digitalWrite(TRANSOPT_PIN_B, LOW);

            last_stepX = 0;
            last_stepY = 0;
            current_pos_x = 0;
            current_pos_y = 0;
            if(Serial) Serial.println("[GCODE] Homing Done");
            return;
        }
        if(num == 30)
        {
            move_complete();
            if(Serial) Serial.println("[GCODE] Returning to software home (0,0,0)...");

            // Ustawiamy pełną prędkość
            stepperX->setSpeed(factor.v_max_x * 0.5);
            stepperY->setSpeed(factor.v_max_y * 0.5);
            stepperX->zero();        
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

            if(parser.HasWord('Z'))
            {
                float val = parser.GetWordValue('Z');
                uint16_t current_ma = (uint16_t)constrain(val, 0, factor.max_current_Z);
                stepperZ->set_current(current_ma);

                if (stepperZ != nullptr) {
                    stepperZ->set_current(current_ma);
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

                // Limity prędkości
                if(vX > factor.v_max_x) { float r = factor.v_max_x/vX; vX*=r; vY*=r; }
                if(vY > factor.v_max_y) { float r = factor.v_max_y/vY; vX*=r; vY*=r; }
            }

            long max_steps = max(abs(diffX), abs(diffY));

            bool is_starting_from_zero = stepperX->isBufferEmpty() && !stepperX->moving() && stepperY->isBufferEmpty() && !stepperY->moving();
            if (is_starting_from_zero)
            {
                steps_in_acceleration = 0;
                current_run_multiplier = 0.1f;
            }

            long steps_sent = 0;
            long sentX = 0, sentY = 0, sentZ = 0;
            

            while (steps_sent < max_steps)
            {
                long active_chunk_size = (steps_in_acceleration < RAMP_STEPS || max_steps - steps_sent < RAMP_STEPS) ? 10 : CHUNK_SIZE;
                long current_chunk = min(active_chunk_size, max_steps - steps_sent);
                long steps_remaining = max_steps - steps_sent;

                steps_in_acceleration += current_chunk;

                if (steps_in_acceleration < RAMP_STEPS) 
                {
                    
                    float ramp_progress = (float)steps_in_acceleration / RAMP_STEPS;
                    current_run_multiplier = V_MIN + (0.9f * ramp_progress);
                }
                else if (!this->next_line_available && steps_remaining < RAMP_STEPS) 
                {
                    float ramp_progress = (float)steps_remaining / RAMP_STEPS;
                    current_run_multiplier = V_MIN + (0.9f * ramp_progress);
                }
                else 
                {
                    current_run_multiplier = V_MAX;
                }

                if(current_run_multiplier > V_MAX) current_run_multiplier = V_MAX;
                if(current_run_multiplier < V_MIN) current_run_multiplier = V_MIN;

                steps_sent += current_chunk;
                
                long target_X = (long)(diffX * ((double)steps_sent / max_steps));
                long target_Y = (long)(diffY * ((double)steps_sent / max_steps));

                long stepX_chunk = target_X - sentX;
                long stepY_chunk = target_Y - sentY;

                sentX = target_X;
                sentY = target_Y;

                while((stepX_chunk != 0 && stepperX->isBufferFull()) || 
                      (stepY_chunk != 0 && stepperY->isBufferFull()))
                {
                    if(this->e_stop) return; 
                    Stepper::moveSteps();
                    delay(1);
                }

                // Wypychanie kroków z przeliczonym mnożnikiem
                if (stepX_chunk != 0) stepperX->addMove(stepX_chunk, vX, true, current_run_multiplier);
                if (stepY_chunk != 0) stepperY->addMove(stepY_chunk, vY, true, current_run_multiplier);
            
                Stepper::moveSteps();

                // Zapamiętanie pozycji
                last_stepX = targetStepX;
                last_stepY = targetStepY;

                current_pos_x = next_pos_x;
                current_pos_y = next_pos_y;
            }
        }
    }
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
        delay(1);
    }

}

void GCode::update_settings(float sx, float sy, float maxZ, float st_mm, float st_rot) {
    factor.v_max_x = sx;
    factor.v_max_y = sy;
    factor.steps_perMM_x = st_mm;
    factor.steps_per_rotation_c = st_rot;
    factor.max_current_Z = maxZ;
    stepperZ->set_current(maxZ);
    Serial.print("Max current = "); 
    Serial.println(maxZ);
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