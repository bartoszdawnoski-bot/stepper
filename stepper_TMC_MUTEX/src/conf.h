//PIO
#define PIO_SELECT_0        pio0
#define PIO_SELECT_1        pio1
#define PIO_SELECT_2        pio2

//Piny dla silnika 1
#define ENABLE_PIN_1        2
#define STEP_PIN_1          1
#define DIR_PIN_1           0 
#define HOLD_PIN_1          4

//Piny dla silnika 2
#define ENABLE_PIN_2        12
#define STEP_PIN_2          11
#define DIR_PIN_2           10 
#define HOLD_PIN_2          9

//Piny dla silnika 3
#define ENABLE_PIN_3        7
#define STEP_PIN_3          6
#define DIR_PIN_3           5
#define HOLD_PIN_3          28

//konfiguracja SPI
#define TMC_CS_PIN_IGNORE   17 //nie uzywany pin tak dla pewnosci lepeij go ustawic
#define TMC_MOSI_PIN        19
#define TMC_MISO_PIN        16
#define TMC_SCK_PIN         18

//Chip Select dla każdego silnika
#define CS_PIN_A            3
#define CS_PIN_B            13
#define CS_PIN_C            8

//Parametr rezystora pomiarowego i prąd sterownika
#define R_SENSE_TMC_PLUS    0.022f
#define R_SENSE_TMC_PRO     0.075f
#define CURRENT_A           1000.0f //prad silnika X | 1000mA = 1A
#define CURRENT_B           1000.0f //prad silnika Y  
#define CURRENT_C           1000.0f // prad silnika Z 

//Piny do obsługi pozostałych elementów
#define E_STOP_PIN          15
#define TRANSOPT_PIN_A      26            
#define TRANSOPT_PIN_B      27    