#include "gcode.h"

GCode::GCode(Stepper* stX, Stepper* stY, Stepper* stZ):
stepperX(stX),
stepperY(stY),
stepperZ(stZ)
{}

GCode::GCode(Stepper* stX, Stepper* stY):
stepperX(stX),
stepperY(stY)
{}

long GCode::steps_from_MM(float mm, float stepsPerMM)
{
    return (long)(mm * stepsPerMM);
}

long GCode::steps_from_rotation(float rotation, float stepsPerRotation)
{
    return (long)(rotation * stepsPerRotation);
}

void GCode::execute_parase()
{
    if(parser.NoWords()) return;
    else if(parser.HasWord('G'))
    {
        int num = (int)parser.GetWordValue('G');
        if(num == 1 || num == 0)
        {
            if(parser.HasWord('X'))
            {
                stepX = steps_from_MM(parser.GetWordValue('X'), factor.steps_perMM_x);
            }

            if (parser.HasWord('Y'))
            {
                rotation = parser.GetWordValue('Y');
                stepY = steps_from_rotation(rotation, factor.steps_per_rotation_c);
            }
            

            if (parser.HasWord('F'))
            {
                float feed_rate_mm_min = parser.GetWordValue('F');
                if (stepX != 0 || stepY != 0)
                {
                    
                    if (feed_rate_mm_min <= 0.0f) feed_rate_mm_min = 1.0f;
                    float speed_mm_sec = feed_rate_mm_min / factor.seconds_in_minute;

                    float dist_mm_x = (float)abs(stepX) / factor.steps_perMM_x;
                    float dist_unit_y = (float)abs(stepY) / factor.steps_per_rotation_c;

                    float total_dist_mm = sqrtf(dist_mm_x * dist_mm_x + dist_unit_y * dist_unit_y);
                    if (total_dist_mm > 0.0f)
                    {
                        fSpeedX = speed_mm_sec * factor.steps_perMM_x * (dist_mm_x / total_dist_mm);
                        fSpeedY = speed_mm_sec * factor.steps_per_rotation_c * (dist_unit_y / total_dist_mm);
                    }

                    float ratio_X = 1.0f;
                    float ratio_Y = 1.0f;

                    if (fSpeedX > factor.v_max_x) {
                        ratio_X = factor.v_max_x / fSpeedX;
                    }
                     if (fSpeedY > factor.v_max_y) {
                        ratio_Y = factor.v_max_y / fSpeedY;
                    }

                    float final_correction_ratio = fminf(ratio_X, ratio_Y);

                    fSpeedX *= final_correction_ratio;
                    fSpeedY *= final_correction_ratio;
                    stepperX->setSpeed(fSpeedX);
                    stepperY->setSpeed(fSpeedY);

                }
                else
                {
                    stepperX->setSpeed(0);
                    stepperY->setSpeed(0);
                }
            }

            if(stepX != 0 || stepY != 0)
            {
                Serial.print("Silnik X speed: "); Serial.println(fSpeedX);
                Serial.print("Silnik Y speed: "); Serial.println(fSpeedY);
                Serial.print("Silnik X kroki: "); Serial.println(stepX);
                Serial.print("Silnik Y kroki: "); Serial.println(stepY);
                stepperX->setSteps(stepX);
                stepperY->setSteps(stepY);
                Stepper::moveSteps();
            }

        }
        else if(num == 28)
        {
            stepperX->zero();
            stepperY->zero();
        }
    }
}

void GCode::processLine(const String& line)
{
    if(line.length() == 0 || line[0] == ';') return;

    char buffer[128];
    line.toCharArray(buffer, 128);

    parser.ParseLine(buffer);
    execute_parase();
}

void GCode::move_complete()
{
    while(stepperX->moving() || stepperY->moving() || stepperZ->moving())
    {
        delay(1);
    }
    long stepX = 0;
    long stepY = 0;
    float rotation = 0.0f;
    float fSpeedX = 0.0f;
    float fSpeedY = 0.0f;
}