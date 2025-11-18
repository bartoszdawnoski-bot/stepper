#include <ArduinoJson.h>


//ustawiania steppera ilosc_krokow_na_obrot * mikrokroki
const float STEPS_PER_MM_X = 1.0f;
const float STEPS_PER_MM_Y = 1.0f;
const float STEPS_PER_MM_Z = 1.0f;

float currentX;
float currentY;
float currentZ;

bool absolute_mode = true;

//funkcja do szukania wartosci po literze
bool parase(const String& line, char code, float& value)
{
    int index = line.indexOf(code);
    if(index == -1) return false;
    value = line.substring(index).toFloat();
    return true;
}

void processSteps(String gcodeLine)
{
    if
}




