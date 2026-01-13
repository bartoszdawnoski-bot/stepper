#include "gcode.h"


// Konstruktory dla 2 lub 3 silników
GCode::GCode(Stepper* stX, Stepper* stY, Stepper* stZ):
stepperX(stX),
stepperY(stY),
stepperZ(stZ)
{}

GCode::GCode(Stepper* stX, Stepper* stY):
stepperX(stX),
stepperY(stY),
stepperZ(nullptr)
{}

// Konwersja milimetrów na kroki
long GCode::steps_from_MM(float mm, float stepsPerMM)
{
    int microsteps = 1;
    if (this->stepperX) {
        microsteps = this->stepperX->get_microsteps(); 
    }
    return (long)(mm * stepsPerMM * microsteps);
}

// Konwersja obrotów na kroki
long GCode::steps_from_rotation(float rotation, float stepsPerRotation)
{
    int microsteps = 1;
    if (this->stepperY) {
        microsteps = this->stepperY->get_microsteps();
    }
    return (long)(rotation * stepsPerRotation * microsteps);
}

// Główna funkcja wykonawcza parsująca i uruchamiająca silniki
void GCode::execute_parse()
{
    // Jeśli linia pusta, wyjdź
    if(parser.NoWords()) return;

    // Statyczna zmienna do zapamiętywania prędkości między wywołaniami
    static float current_feedrate_mm_min = 500.0f;

    
    // Obsługa komend G
    if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        if(num == 90) { relative_mode = false; return; }
        if(num == 91) { relative_mode = true; return; }
        if(num == 28)
        {
            stepperX->setSpeed(factor.v_max_x);
            stepperY->setSpeed(factor.v_max_y);
            stepperX->zero();
            stepperY->zero();

            last_stepX = 0;
            last_stepY = 0;
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
            long targetStepX = last_stepX;
            long targetStepY = last_stepY;
            long targetStepZ = last_stepZ;

            if(parser.HasWord('X'))
            {
                float val = parser.GetWordValue('X');
                long steps = steps_from_MM(val, factor.steps_perMM_x);
                if (relative_mode) targetStepX = last_stepX + steps;
                else targetStepX = steps;
            }

            if (parser.HasWord('Y'))
            {
                float val = parser.GetWordValue('Y');
                long steps = steps_from_rotation(val, factor.steps_per_rotation_c);
                if (relative_mode) targetStepY = last_stepY + steps;
                else targetStepY = steps;
            }

            if (parser.HasWord('Z') && stepperZ != nullptr)
            {
                float val = parser.GetWordValue('Z');
                long steps = steps_from_rotation(val, factor.steps_per_rotation_z);
                if (relative_mode) targetStepY = last_stepY + steps;
                else targetStepY = steps;
            }

            long diffX = targetStepX - last_stepX;
            long diffY = targetStepY - last_stepY;
            long diffZ = targetStepZ - last_stepZ;

            // Obliczanie DELTA 
            long deltaX = abs(diffX);
            long deltaY = abs(diffY);
            long deltaZ = abs(diffZ);

            // Jeśli brak ruchu, kończymy
            if (deltaX == 0 && deltaY == 0 && deltaZ == 0) return;

            // Zamiana F (mm/min) na mm/sec
            float speed_mm_sec = current_feedrate_mm_min / factor.seconds_in_minute;

            //długość wektora w mm
            float dist_mm_x = (float)deltaX / factor.steps_perMM_x;
            float dist_unit_y = (float)deltaY / factor.steps_per_rotation_c; // Zakładamy jednostkę obrotu jako ekwiwalent
            float dist_unit_z = (float)deltaZ / factor.steps_per_rotation_z;

            // Całkowita droga do przebycia
            float total_dist_mm = sqrtf(dist_mm_x * dist_mm_x + dist_unit_y * dist_unit_y + dist_unit_z * dist_unit_z);

            float fSpeedX = 0;
            float fSpeedY = 0;
            float fSpeedZ = 0;

            if (total_dist_mm > 0.0001f) // Zabezpieczenie przed dzieleniem przez zero
            {
                // Prędkość wypadkowa rozłożona na osie Vx = V * (dx / d)
                fSpeedX = speed_mm_sec * factor.steps_perMM_x * (dist_mm_x / total_dist_mm);
                fSpeedY = speed_mm_sec * factor.steps_per_rotation_c * (dist_unit_y / total_dist_mm);
                fSpeedZ = speed_mm_sec * factor.steps_per_rotation_z * (dist_unit_z/ total_dist_mm);
            }

            //Ograniczenia prędkości 
            float ratio_X = 1.0f;
            float ratio_Y = 1.0f;
            float ratio_Z = 1.0f;

            if (fSpeedX > factor.v_max_x) ratio_X = factor.v_max_x / fSpeedX;
            if (fSpeedY > factor.v_max_y) ratio_Y = factor.v_max_y / fSpeedY;
            if (fSpeedZ > factor.v_max_z) ratio_Z = factor.v_max_z / fSpeedZ;

            // Skalujemy obie osie tak samo
            float correction;
            (this->stepperZ != nullptr) ? fminf(ratio_X, fminf(ratio_Y, ratio_Z)) : correction = fminf(ratio_X, ratio_Y);
            fSpeedX *= correction;
            fSpeedY *= correction;
            fSpeedZ *= correction; 

            if (deltaX != 0) {
                stepperX->setSpeed(fSpeedX); 
                stepperX->setSteps(diffX); 
            }

            if (deltaY != 0) {
                stepperY->setSpeed(fSpeedY);
                stepperY->setSteps(diffY);
            }

            if (stepperZ != nullptr && deltaZ != 0) {
                stepperZ->setSpeed(fSpeedZ);
                stepperZ->setSteps(diffZ);
            }
            
            if(Serial) 
            {
                Serial.print("MOVE -> X:"); Serial.print(targetStepX);
                Serial.print(" Y:"); Serial.print(targetStepY);
                if(stepperZ != nullptr) Serial.print(" Z:"); Serial.print(targetStepZ);
                Serial.print(" F:"); Serial.println(current_feedrate_mm_min);
                Serial.print(" Speed X:"); Serial.println(fSpeedX);
                Serial.print(" Speed Y:"); Serial.println(fSpeedY);
                if(stepperZ != nullptr) Serial.print(" Speed Y:"); Serial.println(fSpeedZ);
            }

            Stepper::moveSteps();
            //Zapamiętanie pozycji
            last_stepX = targetStepX;
            last_stepY = targetStepY;
            last_stepZ = targetStepZ;
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
    while(stepperX->moving() || stepperY->moving() || (stepperZ != nullptr && stepperZ->moving()))
    {
        if(digitalRead(E_STOP_PIN) == HIGH)
        {
            stepperX->e_stop();
            stepperY->e_stop();
            if(stepperZ != nullptr) stepperZ->e_stop();
            this->e_stop = true;
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

    // Zerowanie zmiennych lokalnych po ruchu
    stepX = 0;
    stepY = 0;
    stepZ = 0;
    rotation = 0.0f;
    fSpeedX = 0.0f;
    fSpeedY = 0.0f;
    fSpeedZ = 0.0f;
    last_stepX = 0;
    last_stepY = 0;
    last_stepZ = 0;
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