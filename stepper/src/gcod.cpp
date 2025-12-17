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
    return (long)(mm * stepsPerMM);
}

// Konwersja obrotów na kroki
long GCode::steps_from_rotation(float rotation, float stepsPerRotation)
{
    return (long)(rotation * stepsPerRotation);
}

// Główna funkcja wykonawcza parsująca i uruchamiająca silniki
void GCode::execute_parse()
{
    // Jeśli linia pusta, wyjdź
    if(parser.NoWords()) return;

    // Statyczna zmienna do zapamiętywania prędkości między wywołaniami (Modalność F)
    // Domyślnie np. 500 mm/min
    static float current_feedrate_mm_min = 500.0f;

    // Obsługa komend G
    if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        if (num == 90) { relative_mode = false; return; }
        if (num == 91) { relative_mode = true; return; }
        if(num == 28)
        {
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
            
            // C. Obliczanie DELTA (ile kroków do zrobienia teraz)
            long deltaX = abs(targetStepX - last_stepX);
            long deltaY = abs(targetStepY - last_stepY);

            // Jeśli brak ruchu, kończymy
            if (deltaX == 0 && deltaY == 0) return;

            // Zamiana F (mm/min) na mm/sec
            float speed_mm_sec = current_feedrate_mm_min / factor.seconds_in_minute;

            //długość wektora w mm
            float dist_mm_x = (float)deltaX / factor.steps_perMM_x;
            float dist_unit_y = (float)deltaY / factor.steps_per_rotation_c; // Zakładamy jednostkę obrotu jako ekwiwalent
            
            // Całkowita droga do przebycia
            float total_dist_mm = sqrtf(dist_mm_x * dist_mm_x + dist_unit_y * dist_unit_y);

            float fSpeedX = 0;
            float fSpeedY = 0;

            if (total_dist_mm > 0.0001f) // Zabezpieczenie przed dzieleniem przez zero
            {
                // Prędkość wypadkowa rozłożona na osie Vx = V * (dx / d)
                fSpeedX = speed_mm_sec * factor.steps_perMM_x * (dist_mm_x / total_dist_mm);
                fSpeedY = speed_mm_sec * factor.steps_per_rotation_c * (dist_unit_y / total_dist_mm);
            }

            //Ograniczenia prędkości 
            float ratio_X = 1.0f;
            float ratio_Y = 1.0f;

            if (fSpeedX > factor.v_max_x) ratio_X = factor.v_max_x / fSpeedX;
            if (fSpeedY > factor.v_max_y) ratio_Y = factor.v_max_y / fSpeedY;

            // Skalujemy obie osie tak samo
            float correction = fminf(ratio_X, ratio_Y);
            fSpeedX *= correction;
            fSpeedY *= correction;

            // Wykonanie
            stepperX->setSpeed(fSpeedX);
            stepperY->setSpeed(fSpeedY);
            
            stepperX->setSteps(targetStepX);
            stepperY->setSteps(targetStepY);

            if(Serial) 
            {
                Serial.print("MOVE -> X:"); Serial.print(targetStepX);
                Serial.print(" Y:"); Serial.print(targetStepY);
                Serial.print(" F:"); Serial.println(current_feedrate_mm_min);
            }

            Stepper::moveSteps();
            //Zapamiętanie pozycji
            last_stepX = targetStepX;
            last_stepY = targetStepY;
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
    while(stepperX->moving() || stepperY->moving() || stepperZ->moving())
    {
        delay(1);
    }
    // Zerowanie zmiennych lokalnych po ruchu
    long stepX = 0;
    long stepY = 0;
    float rotation = 0.0f;
    float fSpeedX = 0.0f;
    float fSpeedY = 0.0f;
}

void GCode::update_settings(float sx, float sy, float st_mm, float st_rot) {
    factor.v_max_x = sx;
    factor.v_max_y = sy;
    factor.steps_perMM_x = st_mm;
    factor.steps_per_rotation_c = st_rot;
}