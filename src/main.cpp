#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.cpp>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

extern "C" {
#include "user_interface.h"
#include "color_utils.h"
}

#include "config.h"
#include "memory.h"

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, 4, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

#define FLAG_NEW_FRAME (1 << 0)

os_timer_t frameTimer;
volatile uint32_t frame;
volatile uint8_t flags;

global_settings globals;
profile current_profile;
uint16_t frames[TIME_COUNT];

uint16_t time_to_frames(uint8_t time)
{
    if(time <= 80)
    {
        return time * FPS / 16;
    }
    if(time <= 120)
    {
        return time * FPS / 8 - 5 * FPS;
    }
    if(time <= 160)
    {
        return time * FPS / 2 - 50 * FPS;
    }
    if(time <= 190)
    {
        return (time - 130) * FPS;
    }
    if(time <= 235)
    {
        return (2 * time - 320) * FPS;
    }
    if(time <= 245)
    {
        return (15 * time - 3375) * FPS;
    }
    return (60 * time - 14400) * FPS;
}

void convert_to_frames(uint16_t *frames, uint8_t *times)
{
    for(uint8_t i = 0; i < TIME_COUNT; ++i)
    {
        frames[i] = time_to_frames(times[i]);
        Serial.println(String(i)+": "+String(times[i])+" -> "+String(frames[i]));
    }
}

uint32_t autoincrement_to_frames(uint8_t time)
{
    if(time <= 60)
    {
        return time * FPS / 2;
    }
    if(time <= 90)
    {
        return (time - 30) * FPS;
    }
    if(time <= 126)
    {
        return (5 * time / 2 - 165) * FPS;
    }
    if(time <= 156)
    {
        return (5 * time - 480) * FPS;
    }
    if(time <= 196)
    {
        return (15 * time - 2040) * FPS;
    }
    if(time <= 211)
    {
        return (60 * time - 10860) * FPS;
    }
    if(time <= 253)
    {
        return (300 * time - 61500) * FPS;
    }
    if(time == 254) return 18000 * FPS;
    return 21600 * FPS;
}

void convert_bufs()
{
    /* Convert to actual brightness */
    uint8_t *p = strip.getPixels();
    for(uint8_t i = 0; i < LED_COUNT; ++i)
    {
        uint8_t index = i * 3;
        p[index] = actual_brightness(p[index]);
        p[index + 1] = actual_brightness(p[index + 1]);
        p[index + 2] = actual_brightness(p[index + 2]);
    }
}

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
    loadGlobals(globals);
    loadProfile(current_profile, globals.profile_order[globals.n_profile]);

    convert_to_frames(frames, current_profile.devices[0].timing);

    strip.begin();
    server.begin();

    user_init();
}

void loop()
{
    server.handleClient();

    if(flags & FLAG_NEW_FRAME)
    {
        device_profile &device = current_profile.devices[0];
        digital_effect(static_cast<effect>(device.effect), strip.getPixels(), LED_COUNT, 0, frame, frames, device.args,
                       device.colors, device.color_count, device.color_cycles);

        convert_bufs();
        strip.setBrightness(globals.brightness[0]);

        strip.show();
        flags &= ~FLAG_NEW_FRAME;
    }
}
