
#include "SD_controller.h"

SDController::SDController(uint cs, uint rx, uint sck, uint tx) : 
miso_pin(rx), 
cs_pin(cs),
sck_pin(sck),
mosi_pin(tx) 
{
    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);
}

bool SDController::init()
{
    delay(10);
    SPI.setRX(this->miso_pin);
    SPI.setSCK(this->sck_pin);
    SPI.setTX(this->mosi_pin);
    SPI.begin();
    return SD.begin(this->cs_pin);
}

void SDController::new_upload()
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