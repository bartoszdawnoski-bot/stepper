#include "SD_controller.h"

SDController::SDController(uint cs) : cs_pin(cs){}
bool SDController::init()
{
    return SD.begin(this->cs_pin);
}

bool SDController::new_upload()
{
    if(SD.exists(this->filname)) SD.remove(this->filname);
}

bool SDController::add_chunk(uint8_t* data, size_t size)
{
    File file = SD.open(this->filname, O_WRITE | O_APPEND | O_CREAT);
    if(file)
    {
        file.write(data, size);
        file.close();
        return true;
    }
    return false;
}

bool SDController::open_for_reading()
{
    this->gcodeFile = SD.open(filname, FILE_READ);
    return gcodeFile;
}

String SDController::read_line()
{
    if(gcodeFile.available())
    {
        return gcodeFile.readStringUntil('\n');
    }
    return "";
}

bool SDController::file_open()
{
    return gcodeFile;
}

void SDController::close()
{
    if(gcodeFile) gcodeFile.close();
}