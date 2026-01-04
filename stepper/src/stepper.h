/**
 * @file stepper.h
 * @brief Wysokowydajny sterownik silnika krokowego używający Raspberry Pi Pico PIO.
 */
#define STEPPER_H
#ifdef STEPPER_H

#include <Arduino.h>
#include <hardware/pio.h>

#define LENGHT 1 ///< Maksymalna liczba silników na jedną instancję PIO.

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
        mode_16
    };

private:
    PIO PIO_instance; ///< Instancja PIO (pio0, pio1, pio2). 
    uint STEP_PIN, DIR_PIN, ENABLE_PIN, HOLD_PIN, MS1_PIN, MS2_PIN, MS3_PIN;
    Program Program_select;

    // Zmienne wewnętrzne PIO i przerwań
    irq_num_rp2350 IRQ;
    uint irq_num, micro_steps;
    uint SM_counter, SM_speed; ///< Numery maszyn stanów.
    uint offset_counter, offset_speed; ///< Adresy programów w pamięci PIO.

    // Zarządzanie instancjami
    static Stepper* PIO0_Instance[LENGHT];
    static Stepper* PIO1_Instance[LENGHT]; 
    static Stepper* PIO2_Instance[LENGHT];
    static uint pio0_start_mask; ///< Maska bitowa do jednoczesnego startu (pio0).
    static uint pio1_start_mask; ///< Maska bitowa do jednoczesnego startu (pio1).
    static uint pio2_start_mask; ///< Maska bitowa do jednoczesnego startu (pio2).

    float sys_clock;
    float steps_per_second;
    int position, remaining_steps, futurePosition;
    volatile bool isMoving;
    bool Enable;

    /** @brief Wewnętrzna obsługa przerwania z PIO. */
    void PIO_ISR_Handler();

public:
    /**
     * @brief Konstruktor z jawnym wyborem programu PIO.
     */
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold, Program set);

    /**
     * @brief Konstruktor z automatycznym doborem programu PIO.
     */
    Stepper(PIO pio_instance, uint step, uint dir, uint enable, uint hold);

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
    static void PIO0_ISR_handler_static();
    static void PIO1_ISR_handler_static();
    static void PIO2_ISR_handler_static();

    /** @brief Sprawdza czy silnik jest w trakcie ruchu. */
    bool moving();

    /** @brief Konfiguruje piny sterujące mikrokrokami. */
    void set_microSteps_pins(uint ms1, uint ms2, uint ms3);

    /** @brief Ustawia tryb mikrokroków (ustawia stany na pinach MSx). */
    void set_microSteps_mode(Microsteps mode);
    Program getProgram();
    bool setEnable(bool set);
    int getPosition();

    /** @brief Wraca do pozycji zerowej (programowej). */
    void zero();

    /** @brief Ustawia aktualną pozycję jako 0. */
    void setZero();
};

#endif 