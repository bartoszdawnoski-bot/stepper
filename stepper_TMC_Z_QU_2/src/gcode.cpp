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
    if(digitalRead(E_STOP_PIN) == HIGH) 
    {
        stepperX->e_stop();
        stepperY->e_stop();
        if(stepperZ) stepperZ->e_stop();
            this->e_stop = true;
            return;
    }
    // Jeśli linia pusta, wyjdź
    if(parser.NoWords()) return;
    
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
            stepperX->setSpeed(factor.v_max_x);
            stepperY->setSpeed(factor.v_max_y);
            stepperX->zero();
            stepperY->zero();

            last_stepX = 0;
            last_stepY = 0;

            current_pos_x = 0;
            current_pos_y = 0;
            if(Serial) Serial.println("[GCODE] Homed");

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
            // Obliczanie DELTA 
            long targetStepX = steps_from_MM(next_pos_x, factor.steps_perMM_x);
            long targetStepY = steps_from_rotation(next_pos_y, factor.steps_per_rotation_c);
            long targetStepZ = last_stepZ;
            if(stepperZ)
             {
                int micro_z = stepperZ->get_microsteps();
                targetStepZ = (long)(next_pos_z * factor.steps_per_rotation_z * micro_z);
            }

            // Jeśli brak ruchu, kończymy
            long diffX = targetStepX - last_stepX;
            long diffY = targetStepY - last_stepY;
            long diffZ = targetStepZ - last_stepZ;

            // Zamiana F (mm/min) na mm/sec
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

            while((diffX != 0 && stepperX->isBufferFull()) || (diffY != 0 && stepperY->isBufferFull()) || (stepperZ != nullptr && diffZ != 0 && stepperZ->isBufferFull()))
            {
                delay(0); 
            }

            if (diffX != 0) stepperX->addMove(diffX, vX);
            if (diffY != 0) stepperY->addMove(diffY, vY);
            if (stepperZ != nullptr && diffZ != 0) stepperZ->addMove(diffZ, vZ);
            
            /*if(Serial) 
            {
                Serial.print("MOVE -> X:"); Serial.print(targetStepX);
                Serial.print(" Y:"); Serial.print(targetStepY);
                if(stepperZ != nullptr) Serial.print(" Z:"); Serial.print(targetStepZ);
                Serial.print(" F:"); Serial.println(current_feedrate_mm_min);
                Serial.print(" Speed X:"); Serial.println(vX);
                Serial.print(" Speed Y:"); Serial.println(vY);
                if(stepperZ != nullptr) Serial.print(" Speed Y:"); Serial.println(vZ);
            }*/

            // Pchnij kolejkę jeśli silniki stały, to teraz ruszą
            Stepper::moveSteps();
            //Zapamiętanie pozycji
            last_stepX = targetStepX;
            last_stepY = targetStepY;
            last_stepZ = targetStepZ;

            current_pos_x = next_pos_x;
            current_pos_y = next_pos_y;
            current_pos_z = next_pos_z;
        }
    }
}

// Funkcja publiczna do przetwarzania surowego ciągu znaków
void GCode::processLine(const String& line)
{
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
            if(stepperZ != nullptr) stepperZ->e_stop();
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