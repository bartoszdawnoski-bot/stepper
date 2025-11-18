#include <Arduino.h>
#include <math.h>


//ustawiania steppera ilosc_krokow_na_obrot * mikrokroki
const float STEPS_PER_MM_X = 1.0f;
const float STEPS_PER_MM_Y = 1.0f;
const float STEPS_PER_MM_Z = 1.0f;

struct StateMachine{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float speedX = 0.0f;
    float speedY = 0.0f;
    float speedZ = 0.0f;
    float feed_rate = 0.0f;
    bool absolute_mode = true;
};

StateMachine state;
String buffer = "";

bool get_value(String line, char key, float& value)
{
    int index = line.indexOf(key);
    if(index == -1) return false;

    int start = index + 1;
    int end = start;

    while(end < line.length())
    {
        char c = line.charAt(end);
        if(isDigit(c) || c == '.' || c == '-' ) end++;
        else break;
    }

    if(start == end) return false;
    value = line.substring(start, end).toFloat();
    return true;
}

void plan_linear_move(float targetX, float targetY, float targetZ)
{
    float dx = targetX - state.x;
    float dy = targetY - state.y;
    float dz = targetZ - state.z;

    float distance = sqrt(dx*dx + dy*dy + dz*dz);
    if(distance < 0.0001) return;

    float feedrate_mm_s = state.feed_rate / 60;
    float move_time = distance / feedrate_mm_s;

    float stepsX = abs(dx * STEPS_PER_MM_X);
    float stepsY = abs(dy * STEPS_PER_MM_Y);
    float stepsZ = abs(dz * STEPS_PER_MM_Z);

    float speed_steps_x = (move_time > 0) ? (stepsX / move_time) : 0;
    float speed_steps_y = (move_time > 0) ? (stepsY / move_time) : 0;
    float speed_steps_z = (move_time > 0) ? (stepsZ / move_time) : 0;

    state.x = targetX;
    state.speedX = speed_steps_x;
    state.y = targetY;
    state.speedY = speed_steps_y;
    state.z = targetZ;
    state.speedZ = speed_steps_z;

}

//funkcja do szukania wartosci po literze
void parase(String line)
{
    int index = line.indexOf(";");
    if(index != -1) line = line.substring(0, index);
    line.trim();
    if(line.length() == 0) return;

    float f_val;
    if(get_value(line, 'F', f_val)) state.feed_rate = f_val;

    if(line.startsWith("G0") || line.startsWith("G1"))
    {
        float tx = state.x;
        float ty = state.y;
        float tz = state.z;

        bool hasX = get_value(line, 'X', tx);
        bool hasY = get_value(line, 'Y', ty);
        bool hasZ = get_value(line, 'Z', tz);

        if(!state.absolute_mode)
        {
            if(hasX) tx += state.x;
            if(hasY) ty += state.y;
            if(hasZ) tz += state.z;
        }

        plan_linear_move(tx, ty, tz);
    }
    else if(line.startsWith("G90")) state.absolute_mode = true;
    else if (line.startsWith("G91")) state.absolute_mode = false;
    else if(line.startsWith("G28")) 
    {
        state.x = 0;
        state.y = 0;
        state.z = 0;
    }
}




