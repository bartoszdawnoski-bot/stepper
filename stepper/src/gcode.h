/**
 * @file gcode.h
 * @brief Interpreter G-Code i sterownik kinematyki maszyny.
 */
#ifndef GCODE_PARSER_H
#define GCODE_PARSER_H

#include <GCodeParser.h>
#include <WString.h>
#include "stepper.h"

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
    struct Factors {
        const float steps_perMM_x = 100.0f; //wozek
        const float steps_per_rotation_c = 200.0f; //wrzeciono
        const float v_max_x = 100.0f; // predkosc maksymalna silnika X
        const float v_max_y = 100.0f; // predkosc maksymalna silnika y
        const float seconds_in_minute = 60.0f; // sekundy w minucie
    };

    // Zmienne stanu bieżącego ruchu
    long stepX = 0.0f;
    long stepY = 0.0f;
    float rotation = 0.0f;
    float fSpeedX = 0.0f;
    float fSpeedY = 0.0f;
    
    Factors factor;
    Stepper* stepperX; ///< Wskaźnik na silnik wózka.
    Stepper* stepperY; ///< Wskaźnik na silnik wrzeciona.
    Stepper* stepperZ; ///< Wskaźnik na silnik napinacza

    /**
     * @brief Wewnętrzna funkcja wykonująca sparsowaną komendę.
     * Oblicza trajektorię, prędkości i wysyła komendy do sterowników silników.
     */
    void execute_parse();

    long steps_from_MM(float mm, float stepsPerMM);
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
     * @param line Linia tekstu.
     */
    void processLine(const String& line);

    /**
     * @brief Blokuje wykonanie programu do momentu zakończenia ruchu wszystkich silników.
     * Funkcja niezbędna do synchronizacji bufora z fizycznym ruchem.
     */
    void move_complete();
};

#endif