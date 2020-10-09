#include <Arduino.h>

/*
 * Set input pin and output pin definitions etc.
 */
#include "PinDefinitionsAndMore.h"
#undef LED_BUILTIN
#define LED_BUILTIN A0

#define IRMP_PROTOCOL_NAMES 1 // Enable protocol number mapping to protocol strings - requires some FLASH. Must before #include <irmp*>
#define IRMP_SUPPORT_NEC_PROTOCOL        1 // this enables only one protocol
/*
 * After setting the definitions we can include the code and compile it.
 */
#include <irmp.c.h>

IRMP_DATA irmp_data;

void setup()
{
    Serial.begin(115200);
#if defined(__AVR_ATmega32U4__) || defined(SERIAL_USB) || defined(SERIAL_PORT_USBVIRTUAL)
    delay(2000); // To be able to connect Serial monitor after reset and before first printout
#endif
#if defined(ESP8266)
    Serial.println(); // to separate it from the internal boot output
#endif

    pinMode(A0, OUTPUT);
    digitalWrite(A0, HIGH);

    // Just to know which program is running on my Arduino
    Serial.println(F("START " __FILE__ " from " __DATE__ "\r\nUsing library version " VERSION_IRMP));

    irmp_init();
    irmp_irsnd_LEDFeedback(true); // Enable receive signal feedback at LED_BUILTIN

    Serial.print(F("Ready to receive IR signals of protocols: "));
    irmp_print_active_protocols(&Serial);
#if defined(ARDUINO_ARCH_STM32)
    Serial.println(F("at pin " IRMP_INPUT_PIN_STRING));
#else
    Serial.println(F("at pin " STR(IRMP_INPUT_PIN)));
#endif
}

void loop()
{
    /*
     * Check if new data available and get them
     */
    if (irmp_get_data(&irmp_data))
    {
        /*
         * Skip repetitions of command
         */
        if (!(irmp_data.flags & IRMP_FLAG_REPETITION))
        {
            /*
             * Here data is available and is no repetition -> evaluate IR command
             */
            switch (irmp_data.command)
            {
            case 0x48:
                digitalWrite(LED_BUILTIN, HIGH); // will be set to low by IR feedback / irmp_LEDFeedback()
                delay(4000);
                break;
            default:
                break;
            }
        }
//        irmp_result_print(&irmp_data);
    }
}
