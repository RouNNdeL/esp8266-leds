#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.cpp>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

#include "config.h"
#include "memory.h"

#define FLAG_NEW_FRAME (1 << 0)

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, 4, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

extern "C" {
#include "user_interface.h"
#include "color_utils.h"
}

os_timer_t frameTimer;
volatile uint32_t frame;
volatile uint8_t flags;

void timerCallback(void *pArg)
{
    flags |= FLAG_NEW_FRAME;
    frame++;
}

void user_init()
{
    os_timer_setfn(&frameTimer, timerCallback, NULL);
    os_timer_arm(&frameTimer, 10, true);
}

uint8_t char2int(char input);

void setup()
{
#ifdef SERIAL_DEBUG
    Serial.begin(115200);

    Serial.println("");
    Serial.println("|---------------------------|");
    Serial.println("|------ Begin Startup ------|");
    Serial.println("| LED Controller by RouNdeL |");
    Serial.println("|---------------------------|");
#endif /* SERIAL_DEBUG */

    WiFiManager manager;
    manager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    manager.setConfigPortalTimeout(60);
    manager.autoConnect(AP_NAME);

#ifdef SERIAL_DEBUG
    Serial.println("|---------------------------|");
    Serial.println("|------- Start LEDs --------|");
    Serial.println("|---------------------------|");
#endif /* SERIAL_DEBUG */

    EEPROM.begin(SPI_FLASH_SEC_SIZE);
    strip.begin();
    server.begin();

    user_init();
}

void loop()
{
    server.handleClient();

    if(flags & FLAG_NEW_FRAME)
    {
        //TODO: Add actual effect calculations
        strip.show();
        flags &= ~FLAG_NEW_FRAME;
    }
}
