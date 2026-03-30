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
 * * Obsługuje interpolację ruchu, obliczanie prędkości oraz
 * synchronizację osi.
 */
class GCode {
private:
    GCodeParser parser; ///< Obiekt biblioteki parsującej tekst.

    /**
     * @struct Factors
     * @brief Stałe konfiguracyjne kinematyki maszyny.
     */

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

    // tutaj trzymamy stan gdzie maszyna jest i jak szybko jedzie
    float current_pos_x = 0.0f;
    float current_pos_y = 0.0f;
    float current_pos_z = 0.0f;
    bool relative_mode = true; // czy jedziemy relatywnie czy absolutnie
    long last_stepX = 0.0f;
    long last_stepY = 0.0f;
    long last_stepZ = 0.0f;

    Factors factor;
    Stepper* stepperX; ///< Wskaźnik na silnik wózka.
    Stepper* stepperY; ///< Wskaźnik na silnik wrzeciona.
    Stepper* stepperZ; ///< Wskaźnik na silnik napinacza (opcjonalny)

    float current_feedrate_mm_min = 500.0f;
    /**
     * @brief Wewnętrzna funkcja wykonująca sparsowaną komendę.
     * Oblicza trajektorię, prędkości i wysyła komendy do sterowników silników.
     */
    void execute_parse();

    /** @brief Przelicza milimetry na kroki silnika. */
    long steps_from_MM(float mm, float stepsPerMM);

    /** @brief Przelicza stopnie/obroty na kroki silnika. */
    long steps_from_rotation(float rotation, float stepsPerRotation);

public:
    /**
     * @brief Konstruktor dla maszyny 3-osiowej.
     * @param stX Wskaźnik na silnik X.
     * @param stY Wskaźnik na silnik Y.
     * @param stZ Wskaźnik na silnik Z.
     */
    GCode(Stepper* stX, Stepper* stY, Stepper* stZ);

    /**
     * @brief Konstruktor dla maszyny 2-osiowej.
     * @param stX Wskaźnik na silnik X.
     * @param stY Wskaźnik na silnik Y.
     */
    GCode(Stepper* stX, Stepper* stY);

    /**
     * @brief Przetwarza pojedynczą linię tekstu G-Code.
     * @param line Linia tekstu (np. "G1 X100 F500").
     */
    void processLine(const String& line);

    /**
     * @brief Blokuje wykonanie programu do momentu zakończenia ruchu wszystkich silników.
     * Funkcja niezbędna do synchronizacji bufora z fizycznym ruchem.
     */
    void move_complete();

    /**
     * @brief Aktualizuje ustawienia kinematyki w locie (np. po wczytaniu configu).
     */
    void update_settings(float sx, float sy, float sz, float st_mm, float st_rot, float st_br);

    bool is_em_stopped();

    void em_stopp();

    void em_stopp_f() ;
};

#endif