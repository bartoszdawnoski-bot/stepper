#define SD_CONTROLLER_H
#ifdef SD_CONTROLLER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

class SDController {
private:
    uint cs_pin;
    uint miso_pin, sck_pin, mosi_pin ;
    const char* filname = "/winding.gcode";
    File gcodeFile;

public:
    SDController(uint cs, uint rx, uint sck, uint tx);
    bool init();
    void new_upload();
    bool add_chunk(uint8_t* data, size_t size);
    bool open_for_reading();
    String read_line();
    bool file_open();
    void close();
};

#endif