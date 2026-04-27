#include "gcode.h"


// Konstruktory dla 2 lub 3 silników
GCode::GCode(Stepper* stX, Stepper* stY, Stepper* stZ):
stepperX(stX),
stepperY(stY),
stepperZ(stZ),
current_pos_x(0.0f), 
current_pos_y(0.0f), 
current_pos_z(0.0f)
{}

GCode::GCode(Stepper* stX, Stepper* stY):
stepperX(stX),
stepperY(stY),
stepperZ(nullptr),
current_pos_x(0.0f), 
current_pos_y(0.0f), 
current_pos_z(0.0f)
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
            last_stepZ = 0;
            current_pos_x = 0.0f; 
            current_pos_y = 0.0f; 
            current_pos_z = 0.0f;
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
            last_stepZ = 0;
            current_pos_x = 0;
            current_pos_y = 0;
            current_pos_z = 0;
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
            stepperY->zero();          
            move_complete();

            current_pos_x = 0;
            current_pos_y = 0;
            current_pos_z = 0;
            last_stepX = 0;
            last_stepY = 0;
            last_stepZ = 0;
            
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
            float next_pos_z = current_pos_z;

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

            if (parser.HasWord('Z') && stepperZ != nullptr)
            {
                float val = parser.GetWordValue('Z');
                if (relative_mode) next_pos_z += val; else next_pos_z = val;
            }

            float delta_mm_x = next_pos_x - current_pos_x;
            float delta_unit_y = next_pos_y - current_pos_y;
            float delta_unit_z = next_pos_z - current_pos_z;

            float total_dist = sqrtf(delta_mm_x*delta_mm_x + delta_unit_y*delta_unit_y + delta_unit_z*delta_unit_z);
            long targetStepX = steps_from_MM(next_pos_x, factor.steps_perMM_x);
            long targetStepY = steps_from_rotation(next_pos_y, factor.steps_per_rotation_c);
            long targetStepZ = last_stepZ;

            if(stepperZ)
             {
                int micro_z = stepperZ->get_microsteps();
                targetStepZ = (long)(next_pos_z * factor.steps_per_rotation_z * micro_z);
            }

            long diffX = targetStepX - last_stepX;
            long diffY = targetStepY - last_stepY;
            long diffZ = targetStepZ - last_stepZ;

            if (diffX == 0 && diffY == 0 && diffZ == 0) 
            {
                current_pos_x = next_pos_x;
                current_pos_y = next_pos_y;
                current_pos_z = next_pos_z;
                return; 
            }

            float vX = 0, vY = 0, vZ = 0;
            if (total_dist > 0.00001f)
            {
                float speed_unit_sec = current_feedrate_mm_min / 60.0f;
                
                int micro_x = (stepperX) ? stepperX->get_microsteps() : 1;
                int micro_y = (stepperY) ? stepperY->get_microsteps() : 1;
                int micro_z = (stepperZ) ? stepperZ->get_microsteps() : 1;

                // Prędkość w krokach/s = (Prędkość w mm/s) * (Udział osi) * (Kroki/mm)
                vX = speed_unit_sec * (fabs(delta_mm_x) / total_dist) * factor.steps_perMM_x * micro_x;
                vY = speed_unit_sec * (fabs(delta_unit_y) / total_dist) * factor.steps_per_rotation_c * micro_y;
                if(stepperZ) vZ = speed_unit_sec * (fabs(delta_unit_z) / total_dist) * factor.steps_per_rotation_z * micro_z;

                // Limity prędkości
                if(vX > factor.v_max_x) { float r = factor.v_max_x/vX; vX*=r; vY*=r; vZ*=r; }
                if(vY > factor.v_max_y) { float r = factor.v_max_y/vY; vX*=r; vY*=r; vZ*=r; }
                if(stepperZ && vZ > factor.v_max_z) { float r = factor.v_max_z/vZ; vX*=r; vY*=r; vZ*=r; }
            }

            long max_steps = max(abs(diffX), abs(diffY));
            if (stepperZ != nullptr) max_steps = max(max_steps, abs(diffZ));

            bool is_starting_from_zero = stepperX->isBufferEmpty() && !stepperX->moving() && stepperY->isBufferEmpty() && !stepperY->moving();
            if(is_starting_from_zero && stepperZ != nullptr) is_starting_from_zero = stepperZ->isBufferEmpty() && !stepperZ->moving();

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
                else if (!this->next_line_available && steps_in_acceleration < RAMP_STEPS) 
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
                long target_Z = (stepperZ != nullptr) ? (long)(diffZ * ((double)steps_sent / max_steps)) : 0;

                long stepX_chunk = target_X - sentX;
                long stepY_chunk = target_Y - sentY;
                long stepZ_chunk = target_Z - sentZ;

                sentX = target_X;
                sentY = target_Y;
                sentZ = target_Z;

                while((stepX_chunk != 0 && stepperX->isBufferFull()) || 
                      (stepY_chunk != 0 && stepperY->isBufferFull()) || 
                      (stepperZ != nullptr && stepZ_chunk != 0 && stepperZ->isBufferFull()))
                {
                    if(this->e_stop) return; 
                    Stepper::moveSteps();
                    delay(1);
                }

                // Wypychanie kroków z przeliczonym mnożnikiem
                if (stepX_chunk != 0) stepperX->addMove(stepX_chunk, vX, true, current_run_multiplier);
                if (stepY_chunk != 0) stepperY->addMove(stepY_chunk, vY, true, current_run_multiplier);
                if (stepperZ != nullptr && stepZ_chunk != 0) stepperZ->addMove(stepZ_chunk, vZ, true, current_run_multiplier);
            
                Stepper::moveSteps();

                // Zapamiętanie pozycji
                last_stepX = targetStepX;
                last_stepY = targetStepY;
                last_stepZ = targetStepZ;

                current_pos_x = next_pos_x;
                current_pos_y = next_pos_y;
                current_pos_z = next_pos_z;
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
    // Pętla czekająca, jeśli którykolwiek silnik się porusza
   while((stepperX->moving() || !stepperX->isBufferEmpty()) || (stepperY->moving() || !stepperY->isBufferEmpty()) || (stepperZ != nullptr && (stepperZ->moving() || !stepperZ->isBufferEmpty())))
    {
       if(digitalRead(E_STOP_PIN) == HIGH)
        {
            stepperX->e_stop();
            stepperY->e_stop();
            if(stepperZ) stepperZ->e_stop();
            
            this->e_stop = true;
            Serial.println("[MAIN] E-STOP TRIGGERED!");
            return;
        }
        delay(1);
    }

    if(Serial && stepperX->get_tmc() && stepperY->get_tmc())
    {
        // Raport dla osi X
        Serial.print("X Axis -> Load (SG): "); 
        Serial.print(stepperX->get_load()); // Niska wartość to duży opór
        Serial.print(" | Temp/Err: 0x");
        Serial.println(stepperX->get_status(), HEX);

        // Raport dla osi Y
        Serial.print("Y Axis -> Load (SG): "); 
        Serial.print(stepperY->get_load());
        Serial.print(" | Temp/Err: 0x");
        Serial.println(stepperY->get_status(), HEX);
    }

}

void GCode::update_settings(float sx, float sy, float sz, float st_mm, float st_rot, float st_br) {
    factor.v_max_x = sx;
    factor.v_max_y = sy;
    factor.v_max_z = sz;
    factor.steps_perMM_x = st_mm;
    factor.steps_per_rotation_c = st_rot;
    factor.steps_per_rotation_z = st_br;
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