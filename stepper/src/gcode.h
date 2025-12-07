#define GCODE_PARSER_H
#ifdef GCODE_PARSER_H

#include <GCodeParser.h>
#include <WString.h>
#include "stepper.h"

class GCode {
private:
    GCodeParser parser;

    struct Factors {
        const float steps_perMM_x = 100.0f; //wozek
        const float steps_per_rotation_c = 200.0f; //wrzeciono
        const float v_max_x = 100.0f; // predkosc maksymalna silnika X
        const float v_max_y = 100.0f; // predkosc maksymalna silnika y
        const float seconds_in_minute = 60.0f; // sekundy w minucie
    };

    long stepX = 0.0f;
    long stepY = 0.0f;
    float rotation = 0.0f;
    float fSpeedX = 0.0f;
    float fSpeedY = 0.0f;
    
    Factors factor;
    Stepper* stepperX; // wozek
    Stepper* stepperY; // wrzeciono
    Stepper* stepperZ; // napinacz

    void execute_parase();

    long steps_from_MM(float mm, float stepsPerMM);
    long steps_from_rotation(float rotation, float stepsPerRotation);

public:
    GCode(Stepper* stX, Stepper* stY, Stepper* stZ);
    GCode(Stepper* stX, Stepper* stY);
    void processLine(const String& line);
    void move_complete();
};

#endif