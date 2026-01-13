/**
 * @file SD_controller.h
 * @brief Obsługa systemu plików na karcie SD.
 */
#ifndef SD_CONTROLLER_H
#define SD_CONTROLLER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

/**
 * @class SDController
 * @brief Klasa wrapper dla biblioteki SD, obsługująca zapis i odczyt G-Code.
 */
class SDController {
private:
    uint cs_pin; ///< Pin Chip Select.
    uint miso_pin, sck_pin, mosi_pin ; ///< Piny magistrali SPI.
    const char* filname = "/winding.gcode"; ///< Domyślna nazwa pliku roboczego.
    File gcodeFile; ///< Obiekt otwartego pliku.

public:
    /**
     * @brief Konstruktor kontrolera SD.
     * Konfiguruje piny SPI.
     */
    SDController(uint cs, uint rx, uint sck, uint tx);

    /**
     * @brief Inicjalizuje magistralę SPI i kartę SD.
     * @return true jeśli inicjalizacja powiodła się.
     */
    bool init();

    /**
     * @brief Usuwa stary plik roboczy.
     */
    void new_upload();

    /**
     * @brief Dopisuje fragment danych do pliku roboczego.
     * @param data Wskaźnik do bufora danych.
     * @param size Rozmiar danych.
     * @return true jeśli zapis udany.
     */
    bool add_chunk(uint8_t* data, size_t size);

    /**
     * @brief Otwiera plik G-Code w trybie do odczytu.
     * @return true jeśli plik został otwarty.
     */
    bool open_for_reading();

    /**
     * @brief Odczytuje pojedynczą linię z pliku.
     * @return String zawierający linię tekstu.
     */
    String read_line();

    /**
     * @brief Sprawdza, czy plik jest aktualnie otwarty.
     */
    bool file_open();

    /**
     * @brief Zamyka otwarty plik.
     */
    void close();
};

#endif