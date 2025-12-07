#define SD_CONTROLLER_H
#ifdef SD_CONTROLLER_H

#include <Arduino.h>
#include <SD.h>
#include <fcntl.h>

class SDController {
private:
    uint cs_pin;
    const char* filname = "/winding.gcode";
    File gcodeFile;

public:
    SDController(uint cs);
    bool init();
    bool new_upload();
    bool add_chunk(uint8_t* data, size_t size);
    bool open_for_reading();
    String read_line();
    bool file_open();
    void close();
};

#endif