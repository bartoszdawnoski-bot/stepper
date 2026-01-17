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
long GCode::steps_from_MM(float mm, float stepsPerMM) {
    int microsteps = (stepperX) ? stepperX->get_microsteps() : 1;
    return (long)(mm * stepsPerMM * microsteps);
}

long GCode::steps_from_rotation(float rotation, float stepsPerRotation) {
    int microsteps = (stepperY) ? stepperY->get_microsteps() : 1;
    return (long)(rotation * stepsPerRotation * microsteps);
}

// Główna funkcja wykonawcza parsująca i uruchamiająca silniki
void GCode::execute_parse()
{
    // Jeśli linia pusta, wyjdź
    if(parser.NoWords()) return;

    // Obsługa komend G
    if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        
        // Tryb absolutny/relatywny
        if(num == 90) { relative_mode = false; return; }
        if(num == 91) { relative_mode = true; return; }
        if(num == 29)
        {
            flush_buffer(); // Najpierw dokończ obecne ruchy!
            stepperX->setSpeed(factor.v_max_x);
            stepperY->setSpeed(factor.v_max_y);
            if(stepperZ) stepperZ->setSpeed(factor.v_max_z);
            stepperX->zero(); stepperY->zero(); if(stepperZ) stepperZ->zero();
            last_stepX = 0; last_stepY = 0; last_stepZ = 0;
            current_pos_x = 0.0f; current_pos_y = 0.0f; current_pos_z = 0.0f;
            return;
        }
        // Homing (Zerowanie)
        if(num == 28)
        {
            stepperX->setSpeed(factor.v_max_x);
            stepperY->setSpeed(factor.v_max_y);
            if(stepperZ) stepperZ->setSpeed(factor.v_max_z);
            
            stepperX->zero();
            stepperY->zero();
            if(stepperZ) stepperZ->zero();

            last_stepX = 0;
            last_stepY = 0;
            last_stepZ = 0;
            
            // Reset pozycji logicznych
            current_pos_x = 0.0f;
            current_pos_y = 0.0f;
            current_pos_z = 0.0f;

            if(Serial) Serial.println("[GCODE] Homed");
            return;
        }

        // Ruch liniowy G0 / G1
        if(num == 1 || num == 0)
        {
            // Aktualizacja prędkości 
            if (parser.HasWord('F'))
            {
                float val = parser.GetWordValue('F');
                if (val > 0.0f) current_feedrate_mm_min = val;
            }

            //Ustalanie celu na liczbach zmiennoprzecinkowych
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
                if (relative_mode) next_pos_z += val;
                else next_pos_z = val;
            }

            // Delta logiczna 
            float delta_mm_x = next_pos_x - current_pos_x;
            float delta_unit_y = next_pos_y - current_pos_y;
            float delta_unit_z = next_pos_z - current_pos_z;

            long targetStepX = steps_from_MM(next_pos_x, factor.steps_perMM_x);
            long targetStepY = steps_from_rotation(next_pos_y, factor.steps_per_rotation_c);
            long targetStepZ = last_stepZ;
            if(stepperZ) {
                int micro_z = stepperZ->get_microsteps();
                targetStepZ = (long)(next_pos_z * factor.steps_per_rotation_z * micro_z);
            }
            
            long diffX = targetStepX - last_stepX;
            long diffY = targetStepY - last_stepY;
            long diffZ = targetStepZ - last_stepZ;

            pendingX += diffX;
            pendingY += diffY;
            pendingZ += diffZ;

            pending_dist_x += delta_mm_x;
            pending_dist_y += delta_unit_y;
            pending_dist_z += delta_unit_z;

            bool directionChange = false;
            if ((pendingX > 0 && diffX < 0) || (pendingX < 0 && diffX > 0)) directionChange = true;
            if ((pendingY > 0 && diffY < 0) || (pendingY < 0 && diffY > 0)) directionChange = true;

            if (abs(pendingX) >= MIN_STEP_THRESHOLD || abs(pendingY) >= MIN_STEP_THRESHOLD || abs(pendingZ) >= MIN_STEP_THRESHOLD || directionChange)
            {
                // Obliczanie prędkości na podstawie FLOAT (pending_dist) - tak jak chciałeś
                float total_dist = sqrtf(pending_dist_x*pending_dist_x + pending_dist_y*pending_dist_y + pending_dist_z*pending_dist_z);

                if (total_dist > 0.00001f)
                {
                    float speed_unit_sec = current_feedrate_mm_min / 60.0f;
                    
                    // Potrzebujemy mikrokroków do przeliczenia SpeedUnit -> SpeedSteps
                    int micro_x = (stepperX) ? stepperX->get_microsteps() : 1;
                    int micro_y = (stepperY) ? stepperY->get_microsteps() : 1;
                    int micro_z = (stepperZ) ? stepperZ->get_microsteps() : 1;

                    // v_axis = v_total * (d_axis / d_total) * steps_per_unit * microsteps
                    // Tutaj korzystamy z IDEALNEGO d_axis (pending_dist_x), a nie z kroków
                    float vX = speed_unit_sec * (fabs(pending_dist_x) / total_dist) * factor.steps_perMM_x * micro_x;
                    float vY = speed_unit_sec * (fabs(pending_dist_y) / total_dist) * factor.steps_per_rotation_c * micro_y;
                    float vZ = 0;
                    if(stepperZ) vZ = speed_unit_sec * (fabs(pending_dist_z) / total_dist) * factor.steps_per_rotation_z * micro_z;

                    // Limity prędkości
                    if(vX > factor.v_max_x) { float r = factor.v_max_x/vX; vX*=r; vY*=r; vZ*=r; }
                    if(vY > factor.v_max_y) { float r = factor.v_max_y/vY; vX*=r; vY*=r; vZ*=r; }
                    if(stepperZ && vZ > factor.v_max_z) { float r = factor.v_max_z/vZ; vX*=r; vY*=r; vZ*=r; }

                    // Ustawienie silników (KROKI z pendingX, PRĘDKOŚĆ z floatów)
                    if(pendingX != 0) { stepperX->setSpeed(vX); stepperX->setSteps(pendingX); }
                    if(pendingY != 0) { stepperY->setSpeed(vY); stepperY->setSteps(pendingY); }
                    if(stepperZ && pendingZ!=0) { stepperZ->setSpeed(vZ); stepperZ->setSteps(pendingZ); }
                    
                    if(Serial) 
                    {
                        // Debug - odkomentuj jeśli potrzebujesz
                        Serial.print("MOVE -> X:"); Serial.print(pendingX);
                        Serial.print(" Y:"); Serial.print(targetStepY);
                        if(stepperZ && pendingZ!=0) Serial.print(" Z:"); Serial.print(targetStepZ);
                        Serial.print(" Speed X:"); Serial.print(vX);
                        Serial.print(" Speed Y:"); Serial.println(vY);
                        if(stepperZ && pendingZ!=0) Serial.print(" Speed Z:"); Serial.println(vZ);
                    }
                    Stepper::moveSteps();
                }

                // Zerowanie buforów po wykonaniu ruchu
                pendingX = 0; pendingY = 0; pendingZ = 0;
                pending_dist_x = 0.0f; pending_dist_y = 0.0f; pending_dist_z = 0.0f;
            }

            // Aktualizacja pozycji logicznej (GCode wie, gdzie "teoretycznie" jesteśmy)
            last_stepX = targetStepX;
            last_stepY = targetStepY;
            last_stepZ = targetStepZ;

            current_pos_x = next_pos_x;
            current_pos_y = next_pos_y;
            current_pos_z = next_pos_z;
        }
    }
}

void GCode::flush_buffer()
{
    // Wykonaj resztki kroków
    if (pendingX != 0 || pendingY != 0 || pendingZ != 0) {

        // Wersja bezpieczna (stała prędkość):
        if(pendingX != 0) { stepperX->setSpeed(factor.v_max_x/2.0f); stepperX->setSteps(pendingX); }
        if(pendingY != 0) { stepperY->setSpeed(factor.v_max_y/2.0f); stepperY->setSteps(pendingY); }
        if(stepperZ && pendingZ!=0) { stepperZ->setSpeed(factor.v_max_z/2.0f); stepperZ->setSteps(pendingZ); }
        
        Stepper::moveSteps();
        
        pendingX = 0; pendingY = 0; pendingZ = 0;
        pending_dist_x = 0.0f; pending_dist_y = 0.0f; pending_dist_z = 0.0f;
    }
}

// Funkcja publiczna do przetwarzania surowego ciągu znaków
void GCode::processLine(const String& line)
{
    if(line.length() == 0 || line[0] == ';') return;
    char buffer[128];
    line.toCharArray(buffer, 128);
    parser.ParseLine(buffer);
    this->execute_parse();
}

// Funkcja blokująca czeka aż wszystkie silniki zakończą pracę
void GCode::move_complete()
{
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
        Serial.print("X Load: "); Serial.print(stepperX->get_load()); 
        Serial.print(" | Y Load: "); Serial.println(stepperY->get_load());
    }
}

bool GCode::is_moving() {
    return (stepperX->moving() || stepperY->moving() || (stepperZ && stepperZ->moving()));
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
    this->e_stop = false;
}