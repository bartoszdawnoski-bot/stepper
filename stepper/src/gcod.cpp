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
    if(parser.NoWords()) 
    {
       if(Serial) Serial.println("[PARSER] No gcode entered"); 
       return;  
    }
    // Obsługa komend G (Ruch)
    else if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        // Komendy G0 i G1 (Ruch liniowy)
        if(num == 1 || num == 0)
        {
            // Pobierz współrzędną X
            if(parser.HasWord('X'))
            {
                stepX = steps_from_MM(parser.GetWordValue('X'), factor.steps_perMM_x);
            }

            // Pobierz współrzędną Y (jako obrót)
            if (parser.HasWord('Y'))
            {
                rotation = parser.GetWordValue('Y');
                stepY = steps_from_rotation(rotation, factor.steps_per_rotation_c);
            }
            
            // Obsługa prędkości F (Feedrate)
            if (parser.HasWord('F'))
            {
                
                float feed_rate_mm_min = parser.GetWordValue('F');
                if (stepX != 0 || stepY != 0)
                {
                    // Zabezpieczenie przed zerową prędkością
                    if (feed_rate_mm_min <= 0.0f) feed_rate_mm_min = 1.0f;

                    // Przeliczenie mm/min na mm/sec
                    float speed_mm_sec = feed_rate_mm_min / factor.seconds_in_minute;

                    // Obliczenie dystansów dla każdej osi w jednostkach logicznych
                    float dist_mm_x = (float)abs(stepX) / factor.steps_perMM_x;
                    float dist_unit_y = (float)abs(stepY) / factor.steps_per_rotation_c;

                    // Obliczenie całkowitego dystansu wektora ruchu
                    float total_dist_mm = sqrtf(dist_mm_x * dist_mm_x + dist_unit_y * dist_unit_y);
                    if (total_dist_mm > 0.0f)
                    {
                        // Rozkład prędkości na osie proporcjonalnie do dystansu
                        fSpeedX = speed_mm_sec * factor.steps_perMM_x * (dist_mm_x / total_dist_mm);
                        fSpeedY = speed_mm_sec * factor.steps_per_rotation_c * (dist_unit_y / total_dist_mm);
                    }

                    // Ograniczanie prędkości do wartości maksymalnych zdefiniowanych w Factors
                    float ratio_X = 1.0f;
                    float ratio_Y = 1.0f;

                    if (fSpeedX > factor.v_max_x) {
                        ratio_X = factor.v_max_x / fSpeedX;
                    }
                     if (fSpeedY > factor.v_max_y) {
                        ratio_Y = factor.v_max_y / fSpeedY;
                    }

                    // Wybierz najmniejszy współczynnik korekcji, aby zachować synchronizację osi
                    float final_correction_ratio = fminf(ratio_X, ratio_Y);

                    fSpeedX *= final_correction_ratio;
                    fSpeedY *= final_correction_ratio;

                    // Ustaw prędkości silników
                    stepperX->setSpeed(fSpeedX);
                    stepperY->setSpeed(fSpeedY);
                }
                else
                {
                    // Jeśli brak ruchu, wyzeruj prędkość
                    if(Serial) Serial.println("[GCODE] No F value given speed set to 0"); 
                    stepperX->setSpeed(0);
                    stepperY->setSpeed(0);
                }
            }
            // Jeśli jest ruch do wykonania
            if(stepX != 0 || stepY != 0)
            {
                if(Serial)
                {
                    Serial.print("Motor X speed: "); Serial.println(fSpeedX);
                    Serial.print("Motor Y speed: "); Serial.println(fSpeedY);
                    Serial.print("Motor X steeps: "); Serial.println(stepX);
                    Serial.print("Motor Y steeps: "); Serial.println(stepY);
                }
                // Ustaw kroki dla silników (ale jeszcze nie ruszaj)
                stepperX->setSteps(stepX);
                stepperY->setSteps(stepY);

                // Uruchom wszystkie silniki jednocześnie (synchronizacja przez PIO)
                Stepper::moveSteps();
            }
        }
        // Komenda G28 (Bazowanie / Home)
        else if(num == 28)
        {
            stepperX->zero();
            stepperY->zero();
        }
        else
        {
            if(Serial) Serial.println("[GCODE] Unknown command"); 
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