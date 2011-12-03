

// Include files
#include "wirish.h"
#include "utils.h"
#include "esc-control.h"
//#include "ppm-decode.h"


// ASCII escape character
#define ESC       ((uint8)27)


// Servo constants
#define SERVO_MIN 3430
#define SERVO_MAX 6800
#define PPM_CNTS_TO_DEG 0.09
#define SERVO_ANGLE_TO_DUTY 37.44444 
#define SERVO_PIN 16

// Globals
HardwareTimer timer4(4);

// -- setup() and loop() ------------------------------------------------------

void setup() {
    // Set up the LED to blink
    pinMode(BOARD_LED_PIN, OUTPUT);

    // Setup timer4 for PWM
    timer4.setMode(TIMER_CH1, TIMER_PWM);
    timer4.setPrescaleFactor(21);
    pinMode(SERVO_PIN, PWM);

    //ppm decode setup
    SerialUSB.begin();
    while(!isConnected()); //wait till console attaches.
    SerialUSB.println("Hello!");
}


// Force init to be called *first*, i.e. before static object allocation.
// Otherwise, statically allocated objects that need libmaple may fail.
__attribute__((constructor)) void premain() {
    init();
}

int main(void) {
    // init
    setup();

    while (1) {
        toggleLED();
        delay(250);

        while (SerialUSB.available()) {
            uint8 input = SerialUSB.read();
            SerialUSB.println((char)input);

            switch(input) {
            case '\r':
                break;

            case ' ':
                SerialUSB.println("spacebar, nice!");
                break;

            case '?':
            case 'h':
                cmd_print_help();
                break;

            case 't':     
                ppm_decode();
                break;

            case 's':
                cmd_servo_sweep();
                break;

            case 'b':
                cmd_board_info();
                break;

            default: // -------------------------------
                SerialUSB.print("Unexpected byte: 0x");
                SerialUSB.print((int)input, HEX);
                SerialUSB.println(", press h for help.");
                break;
            }

            SerialUSB.print("> ");
        }
    }
    return 0;
}
