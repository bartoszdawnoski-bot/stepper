/**
 * @file gcode.h
 * @brief Interpreter G-Code i sterownik kinematyki maszyny.
 */
#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <GCodeParser.h>
#include <WString.h>
#include "stepper.h"
#include "Wifi_menger.h"
#include "conf.h"

/**
 * @class GCode
 * @brief Klasa odpowiedzialna za parsowanie tekstu G-Code i sterowanie silnikami.
 */
class GCode {
private:
    GCodeParser parser; ///< Obiekt biblioteki parsującej tekst.

    bool e_stop = false;

    struct Factors {
        float steps_perMM_x = 100.0f; // ustawienia wozka
        float steps_per_rotation_c = 200.0f; // ustawienia wrzeciona
        float steps_per_rotation_z = 200.0f; //ustawienia koła hamującego
        float v_max_x = 100.0f; // max predkosc x zeby nie zgubic krokow
        float v_max_y = 100.0f; // max predkosc y
        float v_max_z = 100.0f; // max predkosc Z
        float seconds_in_minute = 60.0f; // sekundy w minucie
    };

    // Stan maszyny
    bool relative_mode = false; // czy jedziemy relatywnie czy absolutnie
    
    // Pozycja w krokach (fizyczna, wysyłana do sterownika)
    long last_stepX = 0;
    long last_stepY = 0;
    long last_stepZ = 0;

    // NOWE: Pozycja rzeczywista (logiczna/idealna)
    // Zapobiega błędom zaokrągleń przy gęstym G-Codzie
    float current_pos_x = 0.0f;
    float current_pos_y = 0.0f;
    float current_pos_z = 0.0f;

    long pendingX = 0;
    long pendingY = 0;
    long pendingZ = 0;

    float pending_dist_x = 0.0f;
    float pending_dist_y = 0.0f;
    float pending_dist_z = 0.0f;
    // Próg: silnik ruszy dopiero, jak uzbiera 200 kroków
    const long MIN_STEP_THRESHOLD = 200;

    // Prędkość posuwu (F) pamiętana między komendami
    float current_feedrate_mm_min = 500.0f;

    Factors factor;
    Stepper* stepperX; ///< Wskaźnik na silnik wózka.
    Stepper* stepperY; ///< Wskaźnik na silnik wrzeciona.
    Stepper* stepperZ; ///< Wskaźnik na silnik napinacza (opcjonalny)

    /**
     * @brief Wewnętrzna funkcja wykonująca sparsowaną komendę.
     */
    void execute_parse();

    /** @brief Przelicza milimetry na kroki silnika (uwzględnia mikrokrok). */
    long steps_from_MM(float mm, float stepsPerMM);

    /** @brief Przelicza stopnie/obroty na kroki silnika (uwzględnia mikrokrok). */
    long steps_from_rotation(float rotation, float stepsPerRotation);

public:
    /**
     * @brief Konstruktor dla maszyny 3-osiowej.
     */
    GCode(Stepper* stX, Stepper* stY, Stepper* stZ);

    /**
     * @brief Konstruktor dla maszyny 2-osiowej.
     */
    GCode(Stepper* stX, Stepper* stY);

    /**
     * @brief Przetwarza pojedynczą linię tekstu G-Code.
     */
    void processLine(const String& line);

    /**
     * @brief Blokuje wykonanie programu do momentu zakończenia ruchu.
     */
    void move_complete();

    /**
     * @brief Aktualizuje ustawienia kinematyki w locie.
     */
    void update_settings(float sx, float sy, float sz, float st_mm, float st_rot, float st_br);

    bool is_em_stopped();
    void em_stopp();

    void flush_buffer();
    
    // Gettery do statusu
    bool is_moving();
    float getX() { return current_pos_x; }
    float getY() { return current_pos_y; }
    float getZ() { return current_pos_z; }
};

#endif