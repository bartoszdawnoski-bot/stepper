#include "gcode.h"

StateMachine state; 

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

void parse(String line)
{
    // Czyszczenie linii
    int commentIdx = line.indexOf(";");
    if(commentIdx != -1) line = line.substring(0, commentIdx);
    line.trim();
    line.toUpperCase();
    if(line.length() == 0) return;

    float f_val;
    if(get_value(line, 'F', f_val)) state.feed_rate = f_val;

    if(line.startsWith("G0") || line.startsWith("G1"))
    {
    
        float tx = state.currentStepX / STEPS_PER_UNIT_X;
        float ty = state.currentStepY / STEPS_PER_UNIT_Y;
        float tz = state.currentStepZ / STEPS_PER_UNIT_Z;

        float val;
        
        if(get_value(line, 'X', val)) tx = state.absolute_mode ? val : (tx + val);
        if(get_value(line, 'Y', val)) ty = state.absolute_mode ? val : (ty + val);
        if(get_value(line, 'Z', val)) tz = state.absolute_mode ? val : (tz + val);

        state.x = lround(tx * STEPS_PER_UNIT_X);
        state.y = lround(ty * STEPS_PER_UNIT_Y);
        state.z = lround(tz * STEPS_PER_UNIT_Z);

        long diffX = state.x - state.currentStepX;
        long diffY = state.y - state.currentStepY;
        long diffZ = state.z - state.currentStepZ;

        if (diffX == 0 && diffY == 0 && diffZ == 0) 
        {
            state.speedX = state.speedY = state.speedZ = 0;
            return;
        }

        state.dirX = (diffX >= 0) ? 1 : -1;
        state.dirY = (diffY >= 0) ? 1 : -1;
        state.dirZ = (diffZ >= 0) ? 1 : -1;

        float uX = abs(diffX) / STEPS_PER_UNIT_X;
        float uY = abs(diffY) / STEPS_PER_UNIT_Y;
        float uZ = abs(diffZ) / STEPS_PER_UNIT_Z;

        float distance = sqrt(uX*uX + uY*uY + uZ*uZ);

        float time_sec = 0;
        if (state.feed_rate > 0) 
        {
            time_sec = distance / (state.feed_rate / 60.0f);
        }

         if (time_sec > 0.001f) { // Unikamy dzielenia przez 0
            state.speedX = lround(abs(diffX) / time_sec);
            state.speedY = lround(abs(diffY) / time_sec);
            state.speedZ = lround(abs(diffZ) / time_sec);
        } 
        else 
        {
            state.speedX = state.speedY = state.speedZ = 0; 
        }
    }
    else if(line.startsWith("G90")) state.absolute_mode = true;
    else if(line.startsWith("G91")) state.absolute_mode = false;
    // Reset pozycji 
    else if(line.startsWith("G92")) 
    {
        
        float val;
        if(get_value(line, 'X', val)) state.currentStepX = lround(val * STEPS_PER_UNIT_X);
        if(get_value(line, 'Y', val)) state.currentStepY = lround(val * STEPS_PER_UNIT_Y);
        if(get_value(line, 'Z', val)) state.currentStepZ = lround(val * STEPS_PER_UNIT_Z);
        state.x = state.currentStepX;
        state.y = state.currentStepY;
        state.z = state.currentStepZ;
    }
}



