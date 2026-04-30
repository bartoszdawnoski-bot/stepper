/**
 * @file stepper.h
 * @brief Wysokowydajny sterownik silnika krokowego używający Raspberry Pi Pico PIO.
 */
#define STEPPER_H
#ifdef STEPPER_H

#include <Arduino.h>
#include <hardware/pio.h>
#include <TMCStepper.h>
#include <SPI.h>
#include "conf.h"
#include "pico/platform.h"

#define LENGHT 2 ///< Maksymalna liczba silników na jedną instancję PIO.
#define MOTION_BUFFER_SIZE 32 ///< Rozmiar bufora ruchów dla jednego silnika

/**
 * @class Stepper
 * @brief Klasa obsługująca silnik krokowy poprzez programowalne wejścia/wyjścia (PIO).
 * * Pozwala na generowanie sygnałów STEP/DIR z bardzo wysoką częstotliwością
 * bez obciążania głównego CPU przerwaniami przy każdym kroku.
 */
class Stepper
{
public:
/** @brief Wybór programu asemblerowego PIO (mapowanie na przerwania IRQ0/IRQ1). */
    enum Program
    {
        PROGRAM_1,
        PROGRAM_2
    };

/** @brief Ustawienia mikrokroków sterownika (np. A4988/DRV8825). */
    enum Microsteps
    {
        mode_1,
        mode_2,
        mode_4,
        mode_8,
        mode_16,
        //dla TMC
        mode_32,
        mode_64,
        mode_128,
        mode_256
    };

/** @brief Struktura przechowująca parametry jednego ruchu */    
struct MotionCommand {
    long steps;
    float speed;
    bool is_sync_move;
    float sync_multiplier;
};


private:
    PIO PIO_instance; ///< Instancja PIO (pio0, pio1, pio2). 
    uint STEP_PIN, DIR_PIN, ENABLE_PIN, HOLD_PIN, MS1_PIN, MS2_PIN, MS3_PIN, CS_PIN, RX_PIN, SCK_PIN, TX_PIN, TROPT_PIN;
    Program Program_select;

    //obsługa sterownika TMC
    TMC5160Stepper* tmc_driver = nullptr;
    bool use_tmc = false;

    // Zmienne wewnętrzne PIO i przerwań
    irq_num_rp2350 IRQ;
    uint irq_num, micro_steps;
    uint SM_counter, SM_speed; ///< Numery maszyn stanów.
    uint offset_counter, offset_speed; ///< Adresy programów w pamięci PIO.
    uint16_t max_current = 1000;
    uint16_t actuall_current;

    // Zarządzanie instancjami
    static Stepper* PIO0_Instance[LENGHT];
    static Stepper* PIO1_Instance[LENGHT]; 
    static Stepper* PIO2_Instance[LENGHT];
    static uint pio0_start_mask; ///< Maska bitowa do jednoczesnego startu (pio0).
    static uint pio1_start_mask; ///< Maska bitowa do jednoczesnego startu (pio1).
    static uint pio2_start_mask; ///< Maska bitowa do jednoczesnego startu (pio2).
    static float global_override;
    static mutex_t spi_mutex;
    static bool spi_mutex_initialized;

    float sys_clock;
    float steps_per_second;
    int position, remaining_steps, futurePosition;

    bool is_zero_seted;
    volatile bool isMoving;
    bool maxSpeed;
    bool Enable;

    float max_speed_chunk;
    float actual_speed_multiplier = 0.1f;
    float accel_speed = 0.1f;
    int max_steps;
    float steps_speed;
    int steps_direction; 
    
    volatile MotionCommand motionBuffer[MOTION_BUFFER_SIZE];
    volatile uint8_t head = 0; // indeks zapisu
    volatile uint8_t tail = 0; // indeks odczytu (w ISR)
    volatile uint8_t items = 0; // licznik elementów

    /** @brief Wewnętrzna obsługa przerwania z PIO. */
    void __not_in_flash_func(PIO_ISR_Handler)();

    /** @brief Funkcja ładująca dane fizycznie do rejestrów PIO */
    void __not_in_flash_func(loadToPIO)(long steps, float speed);

public:
    /**
     * @brief Konstruktor z jawnym wyborem programu PIO.
     */
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, Program set);

    /**
     * @brief Konstruktor z automatycznym doborem programu PIO.
     */
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, uint transopt = 255);

    /**
     * @brief Statyczna metoda uruchamiająca wszystkie przygotowane silniki jednocześnie.
     * Zapewnia idealną synchronizację startu wielu osi.
     */
    static void moveSteps();

    /**
     * @brief Inicjalizuje maszyny stanów PIO i ładuje programy asemblerowe.
     * @return true jeśli inicjalizacja powiodła się.
     */
    bool init();

    /** * @brief Inicjalizuje sterownik TMC po SPI 
     * @param cs_pin Pin Chip Select
     * @param r_sense Rezystor pomiarowy (zwykle 0.075)
     * @param current_ma Prąd w mA
     */
    void initTMC(uint16_t cs, float r_sense, uint16_t current_ma);

    /**
     * @brief Dodaje ruch do bufora zamiast od razu wykonywać
     * @param steps Kroki do wykonania
     * @param speed prędkość z jaką ma zostać wykonany ruch
     * @param steps ture: przymuje mnożnik taktowania z zwenątrz 
     * @param speed wartość mnożnika takowania
     */
    bool addMove(long steps, float speed, bool is_sync = false, float multiplier = 1.0f);
    
    /** 
     * @brief Metody sprawdzające stan bufora 
     */
    bool isBufferFull();
    bool isBufferEmpty();

    /**
     * @brief Ustawia prędkość silnika.
     * @param steps_per_second Prędkość w krokach na sekundę.
     */
    void setSpeed(float steps_per_second);

    /**
     * @brief Planuje wykonanie zadanej liczby kroków.
     * Nie uruchamia ruchu natychmiast - wymagane wywołanie moveSteps().
     * @param steps Liczba kroków (dodatnia lub ujemna określa kierunek).
     */
    void setSteps(long double steps);

    /**
     * @brief Wykonuje ruch natychmiastowo (tylko dla pojedynczego silnika).
     * @deprecated Zaleca się używanie setSteps() + moveSteps() dla synchronizacji.
     */
    void moveThis(long double steps);

    // Statyczne handlery przerwań (wymagane przez API C SDK Pico)
    static void __not_in_flash_func (PIO0_ISR_handler_static)();
    static void __not_in_flash_func (PIO1_ISR_handler_static)();
    static void __not_in_flash_func (PIO2_ISR_handler_static)();

    /** @brief Sprawdza czy silnik jest w trakcie ruchu. */
    bool moving();

    /** @brief Konfiguruje piny sterujące mikrokrokami. */
    void set_microSteps_pins(uint ms1, uint ms2, uint ms3);

    /** @brief Ustawia tryb mikrokroków (ustawia stany na pinach MSx). */
    void set_microSteps_mode(Microsteps mode);

    Program getProgram();
    bool setEnable(bool set);
    int getPosition();
    uint get_microsteps();
    bool get_tmc();

    /** @brief Zwraca obciążenie silnika (StallGuard: 0=max obciążenie, >0 luz) */
    uint16_t get_load();

    /** @brief Zwraca temperaturę sterownika lub flagi błędów */
    uint32_t get_status();

    /** @brief Sprawdza czy sterownik zgłasza przegrzanie */
    bool is_overheated();

    /** @brief Wraca do pozycji zerowej (programowej). */
    void zero();

    /** @brief Ustawia aktualną pozycję jako 0. */
    void setZero();

    /** @brief Funkacja zatrzymania awaryjnego, zatrzymuje maszyny stanów PIO*/
    void e_stop();

    /**@brief Funkcja do zmiany pradu w mA ze sterownikiem TMC */
    void set_current(uint16_t ma);

    /**@brief Funkcja do ustawienia max pradu w mA dla sterownika TMC */
    void set_max_current(uint16_t ma);

    /** * @brief Ustawia globalny mnożnik prędkości (Feedrate Override). 
     * @param override_val Wartość od 0.01 do np. 2.0 (1.0 = 100%, 0.8 = 80%).
     */
    static void set_global_override(float val);

    /** @brief Zwraca stan zasilania silnika (true = włączony) */
    bool isEnabled();

    float get_actuall_current();
};

#endif 