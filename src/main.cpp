#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
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
#define FLAG_PROFILE_UPDATED (1 << 1)

os_timer_t frameTimer;
volatile uint32_t frame;
volatile uint8_t flags;

global_settings globals;
#define increment_profile() globals.n_profile = (globals.n_profile+1)%globals.profile_count

#define refresh_profile() load_profile(&current_profile, globals.profile_order[globals.n_profile]); \
convert_to_frames(frames, current_profile.devices[0].timing)

profile current_profile;
uint16_t frames[TIME_COUNT];
uint32_t auto_increment;

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

void eeprom_init()
{
    EEPROM.begin(SPI_FLASH_SEC_SIZE);
    load_globals(&globals);
    auto_increment = autoincrement_to_frames(globals.auto_increment);
    refresh_profile();
}

#ifdef USER_DEBUG

void ICACHE_FLASH_ATTR debug_info()
{
    String content = "<h2>"+String(AP_NAME)+"</h2>";
    content += "<p>leds_enabled: <b>" + String(globals.leds_enabled) + "</b></p>";
    content += "<p>brightness: <b>[";
    for(uint8_t i : globals.brightness)
    {
        content += String(i) + ",";
    }
    content += "]</b></p>";
    content += "<p>profile_count: <b>" + String(globals.profile_count) + "</b></p>";
    content += "<p>auto_increment: <b>" + String(globals.auto_increment) + "</b></p>";
    content += "<p>auto_increment frames: <b>" + String(auto_increment) + "</b></p>";
    content += "<p>current_profile: <b>" + String(globals.n_profile) + "</b></p>";
    content += "<p>profile_order: <b>[";
    for(uint8_t i : globals.profile_order)
    {
        content += String(i) + ",";
    }
    content += "]</b></p>";
    server.send(200, "text/html", content);
}

#endif

void ICACHE_FLASH_ATTR receive_globals()
{
    if(server.hasArg("plain") && server.arg("plain").length() == GLOBALS_SIZE && server.method() == HTTP_PUT)
    {
        uint8_t previous_profile = globals.profile_order[globals.n_profile];
        uint8_t previous_auto_increment = globals.auto_increment;
        memcpy(&globals, server.arg("plain").begin(), GLOBALS_SIZE);
        if(previous_profile != globals.profile_order[globals.n_profile])
        {
            refresh_profile();
        }
        if(previous_auto_increment != globals.auto_increment)
        {
            auto_increment = autoincrement_to_frames(globals.auto_increment);
        }
        save_globals(&globals);
        server.send(204);
    }
    else
    {
#ifdef USER_DEBUG
        server.send(400, "text/html", "<h2>HTTP 400 Invalid Request</h2>");
#else
        server.send(400);
#endif /* USER_DEBUG */
    }
}

void ICACHE_FLASH_ATTR redirect_to_config()
{
    server.sendHeader("Location", CONFIG_PAGE);
    server.send(301);
}

void ICACHE_FLASH_ATTR receive_profile()
{
    if(server.hasArg("plain") && server.arg("plain").length() == PROFILE_SIZE + 1 && server.method() == HTTP_PUT)
    {
        uint8_t n = server.arg("plain")[0];
        if(globals.n_profile == n)
        {
            memcpy(&current_profile, server.arg("plain").begin() + 1, PROFILE_SIZE);
            flags |= FLAG_PROFILE_UPDATED;
        }
        else
        {
            profile tmp; // NOLINT
            memcpy(&tmp, server.arg("plain").begin() + 1, PROFILE_SIZE);
            save_profile(&tmp, n);
        }
        memcpy(&globals, server.arg("plain").begin(), GLOBALS_SIZE);
    }
    else
    {
#ifdef USER_DEBUG
        server.send(400, "text/html", "<h2>HTTP 400 Invalid Request</h2>");
#else
        server.send(400);
#endif /* USER_DEBUG */
    }
}

uint8_t char2int(char input)
{
    if(input >= '0' && input <= '9')
        return input - '0';
    if(input >= 'A' && input <= 'F')
        return input + 10 - 'A';
    if(input >= 'a' && input <= 'f')
        return input + 10 - 'a';
    return 0;
}

void ICACHE_FLASH_ATTR receive_color()
{
    if(server.hasArg("plain") && server.arg("plain").length() == 6 && server.method() == HTTP_POST)
    {
        const String &c = server.arg("plain");
        uint8_t r = char2int(c[0]) * 16 + char2int(c[1]);
        uint8_t g = char2int(c[2]) * 16 + char2int(c[3]);
        uint8_t b = char2int(c[4]) * 16 + char2int(c[5]);

        if(flags & FLAG_PROFILE_UPDATED)
        {
            save_profile(&current_profile, globals.profile_order[globals.n_profile]);
            flags &= ~FLAG_PROFILE_UPDATED;
        }

        current_profile.devices[0].effect = BREATHE;
        current_profile.devices[0].timing[TIME_FADEIN] = 0;
        current_profile.devices[0].timing[TIME_FADEOUT] = 0;
        current_profile.devices[0].timing[TIME_ON] = 1;
        current_profile.devices[0].timing[TIME_OFF] = 0;
        current_profile.devices[0].color_count = 1;
        current_profile.devices[0].args[ARG_BREATHE_END] = 255;
        current_profile.devices[0].colors[0] = g;
        current_profile.devices[0].colors[1] = r;
        current_profile.devices[0].colors[2] = b;
        convert_to_frames(frames, current_profile.devices[0].timing);

        server.send(200, "application/json", R"({"status": "success"})");
    }
    else
    {
        server.send(400, "text/html", "Invalid request");
    }
}

void ICACHE_FLASH_ATTR handle_root()
{
    server.send(200, "text/html", "<!DOCTYPE html>\n"
            "<html lang=\"en\">\n"
            "<head>\n"
            "    <meta charset=\"UTF-8\">\n"
            "    <title>Title</title>\n"
            "</head>\n"
            "<body>\n"
            "<input name=\"color\" type=\"color\">\n"
            "<script>\n"
            "    document.getElementsByTagName(\"input\")[0].addEventListener(\"change\", function (e) {\n"
            "        var http = new XMLHttpRequest();\n"
            "        var url = \"color\";\n"
            "        http.open(\"POST\", url, true);\n"
            "        http.send(e.target.value.substring(1));\n"
            "    });\n"
            "</script>\n"
            "</body>\n"
            "</html>");
}

void setup()
{
#ifdef USER_DEBUG
    Serial.begin(115200);

    Serial.println("");
    Serial.println("|---------------------------|");
    Serial.println("|------ Begin Startup ------|");
    Serial.println("| LED Controller by RouNdeL |");
    Serial.println("|---------------------------|");
#endif /* USER_DEBUG */

    WiFiManager manager;
#ifndef USER_DEBUG
    manager.setDebugOutput(0);
#endif
    manager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    manager.setConfigPortalTimeout(60);
    manager.autoConnect(AP_NAME);

#ifdef USER_DEBUG
    Serial.println("|---------------------------|");
    Serial.println("|------- Start LEDs --------|");
    Serial.println("|---------------------------|");
#endif /* USER_DEBUG */

#ifdef USER_DEBUG
    server.on("/debug", debug_info);
#endif /* USER_DEBUG */

    server.on("/", handle_root);
    server.on("/config", redirect_to_config);
    server.on("/globals", receive_globals);
    server.on("/profile", receive_profile);
    server.on("/color", receive_color);

    strip.begin();
    server.begin();

    eeprom_init();
    user_init();
}

void loop()
{
    server.handleClient();

    if(flags & FLAG_NEW_FRAME)
    {
        if(auto_increment && frame && frame % auto_increment == 0 && globals.leds_enabled && globals.profile_count > 1)
        {
            if(flags & FLAG_PROFILE_UPDATED)
            {
                save_profile(&current_profile, globals.profile_order[globals.n_profile]);
                flags &= ~FLAG_PROFILE_UPDATED;
            }
            increment_profile();
            refresh_profile();
            frame = 0;
        }

        device_profile &device = current_profile.devices[0];
        digital_effect(static_cast<effect>(device.effect), strip.getPixels(), LED_COUNT, 0, frame, frames, device.args,
                       device.colors, device.color_count, device.color_cycles);

        convert_bufs();
        strip.setBrightness(globals.brightness[0]);

        strip.show();
        flags &= ~FLAG_NEW_FRAME;
    }
}
