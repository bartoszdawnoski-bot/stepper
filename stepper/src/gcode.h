#ifndef GCOD_H
#define GCOD_H
#include <Arduino.h>
#include <math.h>

const float STEPS_PER_UNIT_X = 1.0f;
const float STEPS_PER_UNIT_Y = 1.0f;
const float STEPS_PER_UNIT_Z = 1.0f;

struct StateMachine{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float speedX = 0.0f;
    float speedY = 0.0f;
    float speedZ = 0.0f;

    long currentStepX = 0;
    long currentStepY = 0;
    long currentStepZ = 0;

    int dirX = 1;
    int dirY = 1;
    int dirZ = 1;

    float feed_rate = 0.0f;
    bool absolute_mode = true;
};

extern StateMachine state; 

void parse(String line);
void plan_linear_move(float targetX, float targetY, float targetZ);
bool get_value(String line, char key, float& value);

#endif