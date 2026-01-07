//Użyte PIO
#define PIO_SELECT_0        pio0
#define PIO_SELECT_1        pio1

//Piny dla silnika 1
#define ENABLE_PIN          2
#define STEP_PIN            1
#define DIR_PIN             0 
#define HOLD_PIN            3

//Piny dla silnika 2
#define ENABLE_PIN_2        6
#define STEP_PIN_2          5
#define DIR_PIN_2           4 
#define HOLD_PIN_2          7

//Piny dla silnika 3
#define ENABLE_PIN_3        255
#define STEP_PIN_3          255
#define DIR_PIN_3           255
#define HOLD_PIN_3          255

//konfiguracja SPI
#define TMC_CS_PIN_IGNORE   255 //nie uzywany pin tak dla pewnosci lepeij go ustawic
#define TMC_MOSI_PIN        255
#define TMC_MISO_PIN        255
#define TMC_SCK_PIN         255

//Chip Select dla każdego silnika
#define CS_PIN_A            255
#define CS_PIN_B            255

//Parametr rezystora pomiarowego
#define R_SENSE             0.075f
#define CURRENT_A           1000.0f //prad silnika X sterownika | 1000mA = 1A
#define CURRENT_B           1000.0f //prad silnika Y sterownika 