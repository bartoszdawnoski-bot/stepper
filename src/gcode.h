#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <Arduino.h>
#include <math.h>

#define STEPS_PER_UNIT_X  80.0f   // Wrzeciono 
#define STEPS_PER_UNIT_Y  80.0f   // Oś Y
#define STEPS_PER_UNIT_Z  400.0f  // Wózek 

#define MAX_SAFE_HZ       600.0f


class GCodeParser {
private:
    long currentStepX; //polozenie
    long currentStepY;
    long currentStepZ;

    long targetStepX;  //docelowe polozenie
    long targetStepY;
    long targetStepZ;

    long diffX; // ile krokow trzeba zrobic
    long diffY;
    long diffZ;
   
    float speedX; // predkosci
    float speedY;
    float speedZ;

    float feed_rate;   // F (mm/min)
    bool absolute_mode; // G90 = true, G91 = false
    bool executeMove;  // Flaga czy parser zlecił ruch silnikami?
    bool get_value(String line, char key, float& value);

public:
    GCodeParser();
    void parse(String line);

    float get_speedX();
    long get_stepX();

    float get_speedY();
    long get_stepY();

    float get_speedZ();
    long get_stepZ();

    bool has_move();
    void complete_move(); 
};

#endif