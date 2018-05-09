#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ESP8266httpUpdate.h>

extern "C" {
#include "user_interface.h"
#include "color_utils.h"
}

#include "config.h"
#include "memory.h"

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);
WiFiUDP Udp;

#define FLAG_NEW_FRAME (1 << 0)
#define FLAG_PROFILE_UPDATED (1 << 1)
#define FLAG_MANUAL_COLOR (1 << 2)

os_timer_t frameTimer;
volatile uint32_t frame;
volatile uint8_t flags;

global_settings globals;
#define increment_profile() globals.n_profile = (globals.n_profile+1)%globals.profile_count

#define refresh_profile() load_profile(&current_profile, globals.profile_order[globals.n_profile]); \
convert_to_frames(frames, current_profile.devices[0].timing); flags &= ~FLAG_MANUAL_COLOR

profile current_profile;
uint16_t frames[TIME_COUNT];
uint32_t auto_increment;
uint8_t manual_color[3] = {0x00, 0x00, 0x00};

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
        set_color_manual(p + index, color_brightness(globals.brightness[0], color_from_buf(p + index)));
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

void setStripStatus(uint8_t r, uint8_t g, uint8_t b)
{
    if(!(globals.flags & GLOBALS_FLAG_STATUSES))
        return;
    strip.setBrightness(UINT8_MAX);
    for(led_count_t i = 0; i < LED_COUNT; ++i)
    {
        if(i % 4)
            strip.setPixelColor(i, color_brightness(12, r, g, b));
        else
            strip.setPixelColor(i, r, g, b);
    }
    strip.show();
}

int checkUpdate()
{
    HTTPClient http;
    http.begin(HTTP_UPDATE_HOST, HTTP_UPDATE_PORT, String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID,
               HTTP_UPDATE_HTTPS_FINGERPRINT);
    http.useHTTP10(true);
    http.setTimeout(8000);
    http.setUserAgent(F("ESP8266-http-Update"));
    http.addHeader(F("x-ESP8266-STA-MAC"), WiFi.macAddress());
    http.addHeader(F("x-ESP8266-AP-MAC"), WiFi.softAPmacAddress());
    http.addHeader(F("x-ESP8266-free-space"), String(ESP.getFreeSketchSpace()));
    http.addHeader(F("x-ESP8266-sketch-size"), String(ESP.getSketchSize()));
    http.addHeader(F("x-ESP8266-sketch-md5"), String(ESP.getSketchMD5()));
    http.addHeader(F("x-ESP8266-chip-size"), String(ESP.getFlashChipRealSize()));
    http.addHeader(F("x-ESP8266-sdk-version"), ESP.getSdkVersion());
    http.addHeader(F("x-ESP8266-mode"), F("check"));
    http.addHeader(F("x-ESP8266-version"), String(VERSION_CODE));

    int code = http.GET();
    http.end();
    return code;
}

HTTPUpdateResult update(uint8_t reboot)
{
    setStripStatus(COLOR_MAGENTA);
#if SERIAL_DEBUG
    Serial.println("[OTA] Checking for updates...");
#endif /* SERIAL_DEBUG */
    ESPhttpUpdate.rebootOnUpdate(false);
#if HTTP_UPDATE_HTTPS
    HTTPUpdateResult ret = ESPhttpUpdate.update(HTTP_UPDATE_HOST, HTTP_UPDATE_PORT,
                                                String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID,
                                                String(VERSION_CODE), HTTP_UPDATE_HTTPS_FINGERPRINT);
#else
    HTTPUpdateResult ret = ESPhttpUpdate.update(HTTP_UPDATE_HOST, HTTP_UPDATE_PORT,
                                                String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID,
                                                String(VERSION_CODE));
#endif /* HTTP_UPDATE_HTTPS */
    switch(ret)
    {
        case HTTP_UPDATE_FAILED:
#if SERIAL_DEBUG
            Serial.println("[OTA] Update failed");
#endif /* SERIAL_DEBUG */
            setStripStatus(COLOR_RED);
            yield();
            delay(1000);
            break;
        case HTTP_UPDATE_NO_UPDATES:
#if SERIAL_DEBUG
            Serial.println("[OTA] No update");
#endif
            setStripStatus(rgb(244, 165, 0));
            yield();
            delay(1000);
            break;
        case HTTP_UPDATE_OK:
#if SERIAL_DEBUG
            Serial.println("[OTA] Update successful"); // may not called we reboot the ESP
#endif /* SERIAL_DEBUG */
            setStripStatus(COLOR_GREEN);
            yield();
            delay(1000);
            if(reboot)
            {
#if SERIAL_DEBUG
                Serial.println("[OTA] Rebooting...");
#endif /* SERIAL_DEBUG */
                delay(250);
                ESP.restart();
            }
            break;
    }
    return ret;
}

#if PAGE_DEBUG

void ICACHE_FLASH_ATTR debug_info()
{
    String content = "<h2>" + String(AP_NAME) + "</h2>";
    content += "<p>config_url: <a href=\"" + String(CONFIG_PAGE) + "\">" + CONFIG_PAGE + "</a></p>";
    content += "<p>version_code: <b>" + String(VERSION_CODE) + "</b></p>";
    content += "<p>version_name: <b>" + String(VERSION_NAME) + "</b></p>";
    content += "<p>build_date: <b>" + BUILD_DATE + "</b></p>";
    content += "<p>reset_info: <b>" + ESP.getResetReason() + "</b></p>";
    content += "<p>device_id: <b>" + String(DEVICE_ID) + "</b></p>";
    content += "<p>leds_enabled: <b>" + String(globals.flags & GLOBALS_FLAG_STATUSES) + "</b></p>";
    content += "<p>statuses: <b>" + String(globals.flags & GLOBALS_FLAG_ENABLED) + "</b></p>";
    content += "<p>brightness: <b>[";
    for(uint8_t i = 0; i < DEVICE_COUNT; i++)
    {
        if(i) content += ",";
        content += String(globals.brightness[i]);
    }
    content += "]</b></p>";
    content += "<p>profile_count: <b>" + String(globals.profile_count) + "</b></p>";
    content += "<p>auto_increment: <b>" + String(globals.auto_increment) + "</b></p>";
    content += "<p>auto_increment frames: <b>" + String(auto_increment) + "</b></p>";
    content += "<p>current_profile: <b>" + String(globals.n_profile) + "</b></p>";
    content += "<p>profile_order: <b>[";
    for(uint8_t i = 0; i < PROFILE_COUNT; i++)
    {
        if(i) content += ",";
        content += String(globals.profile_order[i]);
    }
    content += "]</b></p>";
    server.send(200, "text/html", content);
}

#endif /* PAGE_DEBUG */

void ICACHE_FLASH_ATTR receive_globals()
{
    if(server.hasArg("plain") && server.arg("plain").length() == GLOBALS_SIZE * 2 && server.method() == HTTP_PUT)
    {
        uint8_t bytes[GLOBALS_SIZE];
        auto c = server.arg("plain");
        for(uint8_t i = 0; i < sizeof(bytes); ++i)
        {
            bytes[i] = char2int(c[i * 2]) * 16 + char2int(c[i * 2 + 1]);
        }
        uint8_t previous_profile = globals.profile_order[globals.n_profile];
        uint8_t previous_auto_increment = globals.auto_increment;
        memcpy(&globals, bytes, GLOBALS_SIZE);
        if(previous_profile != globals.profile_order[globals.n_profile] || flags & FLAG_MANUAL_COLOR)
        {
            frame = 0;
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
#if SERIAL_DEBUG
        server.send(400, "text/html", "<h2>HTTP 400 Invalid Request</h2>");
#else
        server.send(400);
#endif /* SERIAL_DEBUG */
    }
}

void ICACHE_FLASH_ATTR redirect_to_config()
{
    server.sendHeader("Location", CONFIG_PAGE);
    server.send(301);
}

void ICACHE_FLASH_ATTR receive_profile()
{
    if(server.hasArg("plain") && server.arg("plain").length() == (PROFILE_SIZE + 1) * 2 && server.method() == HTTP_PUT)
    {
        uint8_t bytes[PROFILE_SIZE + 1];
        auto c = server.arg("plain");
        for(uint8_t i = 0; i < sizeof(bytes); ++i)
        {
            bytes[i] = char2int(c[i * 2]) * 16 + char2int(c[i * 2 + 1]);
        }

        if(globals.n_profile == bytes[0])
        {
            memcpy(&current_profile, bytes + 1, PROFILE_SIZE);
            convert_to_frames(frames, current_profile.devices[0].timing);
            flags |= FLAG_PROFILE_UPDATED;
        }
        else
        {
            profile tmp; // NOLINT
            memcpy(&tmp, bytes + 1, PROFILE_SIZE);
            save_profile(&tmp, bytes[0]);
        }
        server.send(204);
    }
    else
    {
#if SERIAL_DEBUG
        server.send(400, "text/html", "<h2>HTTP 400 Invalid Request</h2>");
#else
        server.send(400);
#endif /* SERIAL_DEBUG */
    }
}

void ICACHE_FLASH_ATTR receive_color()
{
    if(server.hasArg("plain") && server.arg("plain").length() == 6 && server.method() == HTTP_POST)
    {
        const String &c = server.arg("plain");
        manual_color[0] = char2int(c[0]) * 16 + char2int(c[1]);
        manual_color[1] = char2int(c[2]) * 16 + char2int(c[3]);
        manual_color[2] = char2int(c[4]) * 16 + char2int(c[5]);

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
        set_color_manual(current_profile.devices[0].colors, grb(color_from_buf(manual_color)));
        convert_to_frames(frames, current_profile.devices[0].timing);

        flags |= FLAG_MANUAL_COLOR;

        auto_increment = 0;

        server.send(200, "application/json", R"({"status": "success"})");
    }
    else
    {
        server.send(400, "text/html", "Invalid request");
    }
}

void ICACHE_FLASH_ATTR change_profile()
{
    if(server.hasArg("plain") && server.arg("plain").length() == 1 && server.method() == HTTP_POST)
    {
        /* We subtract the 1 added previously when creating the webpage */
        uint8_t i = (uint8_t) server.arg("plain")[0] - 1;
        if(flags & FLAG_PROFILE_UPDATED)
        {
            save_profile(&current_profile, globals.profile_order[globals.n_profile]);
            flags &= ~FLAG_PROFILE_UPDATED;
        }
        globals.n_profile = i;
        refresh_profile();

        frame = 0;
        auto_increment = autoincrement_to_frames(globals.auto_increment);

        server.send(200, "application/json", R"({"status": "success"})");
    }
    else
    {
#if SERIAL_DEBUG
        server.send(400, "text/html", "<h2>HTTP 400 Invalid Request</h2>");
#else
        server.send(400);
#endif /* SERIAL_DEBUG */
    }
}

void ICACHE_FLASH_ATTR handle_root()
{
    String r = String(manual_color[0], 16);
    String g = String(manual_color[1], 16);
    String b = String(manual_color[2], 16);
    r = r.length() == 1 ? "0" + r : r;
    g = g.length() == 1 ? "0" + g : g;
    b = b.length() == 1 ? "0" + b : b;
    String content =
            R"(<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>)" +
            String(AP_NAME) + "</title></head><body><h2>" + AP_NAME +
            R"(</h2><p>You can change profiles and set a static color for your LEDs, for more advanced configuration click <a href="/config">here</a></p> <input id="a" type="color" value="#)" +
            r + g + b + R"("> <select id="b">)";
    for(uint8_t i = 0; i < globals.profile_count; ++i)
    {
        String selected = i == globals.n_profile ? "selected" : "";
        /* We add 1 to avoid sending a NULL which is a String termination character in C.
         * The profile_n endpoint will then subtract that 1 to account for that*/
        content += "<option value=\"" + String(i + 1) + "\" " + selected + ">" + String(globals.profile_order[i]) +
                   "</option>";
    }
    content += R"(</select><p><a href="/restart">Restart device</a></p><p><a href="/update">Check for updates</a></p> <script>document.getElementById("a").addEventListener("change",function(e){var t=new XMLHttpRequest;t.open("POST","/color",!0),t.send(e.target.value.substring(1))}),document.getElementById("b").addEventListener("change",function(e){var t=new XMLHttpRequest;t.open("POST","/profile_n",!0),t.send(new Uint8Array([e.target.value]))});</script> </body></html>)";
    server.send(200, "text/html", content);
}

void ICACHE_FLASH_ATTR restart()
{
    server.send(200, "text/html",
                "<p>Your device is restarting...</p><p><a href=\"/\">Go back to configuration panel</a></p>");
    delay(500);
    ESP.restart();
}

void ICACHE_FLASH_ATTR send_json()
{
    String profile_array = "[";
    for(uint8_t i = 0; i < globals.profile_count; ++i)
    {
        if(i) profile_array += ",";
        profile_array += globals.profile_order[i];
    }
    profile_array += "]";
    String content = R"({"ap_name":")" + String(AP_NAME) +
                     R"(","version_code":)" + String(VERSION_CODE) +
                     R"(,"version_name":")" + String(VERSION_NAME) +
                     R"(","device_id":)" + String(DEVICE_ID) +
                     ",\"leds_enabled\":" + (globals.flags & GLOBALS_FLAG_ENABLED ? "true" : "false") +
                     ",\"current_profile\":" + String(globals.n_profile) +
                     ",\"flags\":" + String(flags) +
                     ",\"profiles\": " + profile_array + "}";
    server.send(200, "application/json", content);
}

void ICACHE_FLASH_ATTR manual_update_check()
{
    auto code = checkUpdate();
    String statusString;
    switch(code)
    {
        case HTTP_CODE_NO_CONTENT:
            statusString = "Update found, click <a href=\"/apply_update\">here</a> to download and apply the update";
            break;
        case HTTP_CODE_NOT_MODIFIED:
            statusString = "No updates found, <a href=\"/\">return to the main page<a/>";
            break;
        default:
            statusString = "Update check failed: HTTP " + code;
            break;
    }
    server.send(200, "text/html", statusString);
}

void ICACHE_FLASH_ATTR apply_update()
{
    server.send(200, "text/html",
                "<p>The device will now download the update and restart to apply the changes</p><p><a href=\"/\">Main page</a></p>");
    delay(500);
    update(1);
}

void on_disconnected(const WiFiEventStationModeDisconnected &event)
{
    ESP.restart();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
    setStripStatus(COLOR_CYAN);
}

void handleUdp()
{
    int packetSize = Udp.parsePacket();
    if(packetSize)
    {
        uint8_t buffer[32];
        int len = Udp.read(buffer, 32);
        if(len > 0)
        {
            buffer[len] = 0;
        }
        if(String((char *) buffer) == UDP_DISCOVERY_MSG)
        {
            Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
            String resp = R"({"message":")" + String(UDP_DISCOVERY_RESPONSE) + R"(", "name":")" + AP_NAME + "\"}";
            char bytes[resp.length()];
            resp.toCharArray(bytes, resp.length() + 1);
            Udp.write(bytes, resp.length());
            Udp.endPacket();
        }
    }
}

void recover()
{
    EEPROM.begin(SPI_FLASH_SEC_SIZE);

    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        globals.brightness[i] = 0xff;
    }
    globals.auto_increment = 0;
    globals.profile_order[0] = 0;
    globals.profile_count = 1;
    globals.flags = GLOBALS_FLAG_ENABLED | GLOBALS_FLAG_STATUSES;
    globals.n_profile = 0;

    current_profile.devices[0].effect = PIECES;
    current_profile.devices[0].color_count = 2;
    current_profile.devices[0].color_cycles = 1;
    current_profile.devices[0].timing[TIME_OFF] = 0;
    current_profile.devices[0].timing[TIME_FADEIN] = 0;
    current_profile.devices[0].timing[TIME_ON] = 60;
    current_profile.devices[0].timing[TIME_FADEOUT] = 30;
    current_profile.devices[0].timing[TIME_ROTATION] = 90;
    current_profile.devices[0].timing[TIME_DELAY] = 0;
    current_profile.devices[0].args[ARG_BIT_PACK] = DIRECTION | SMOOTH;
    current_profile.devices[0].args[ARG_PIECES_PIECE_COUNT] = 6;
    current_profile.devices[0].args[ARG_PIECES_COLOR_COUNT] = 2;
    set_color_manual(current_profile.devices[0].colors, grb(COLOR_RED));
    set_color_manual(current_profile.devices[0].colors + 3, grb(COLOR_BLUE));

    convert_to_frames(frames, current_profile.devices[0].timing);

    save_globals(&globals);
    save_profile(&current_profile, 0);
}

void setup()
{
#if SERIAL_DEBUG
    Serial.begin(115200);

    Serial.println("");
    Serial.println("|---------------------------|");
    Serial.println("|------ Begin Startup ------|");
    Serial.println("| LED Controller by RouNdeL |");
    Serial.println("|---------------------------|");

    Serial.println("Version Name: " + String(VERSION_NAME));
    Serial.println("Version Code: " + String(VERSION_CODE));
    Serial.println("Build Date: " + BUILD_DATE);
    Serial.println("Device Id: " + String(DEVICE_ID));
#endif /* SERIAL_DEBUG */

#if RECOVER
    recover();
#else
    eeprom_init();
#endif /* RECOVER */

    strip.begin();
    setStripStatus(COLOR_RED);

    WiFiManager manager;
#if !SERIAL_DEBUG
    manager.setDebugOutput(0);
#endif /* SERIAL_DEBUG */
    manager.setAPCallback(configModeCallback);
    manager.setAPStaticIPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
    manager.setConfigPortalTimeout(300);
    manager.autoConnect(AP_NAME);

    update(1);

#if SERIAL_DEBUG
    Serial.println("|---------------------------|");
    Serial.println("|------- Start LEDs --------|");
    Serial.println("|---------------------------|");
#endif /* SERIAL_DEBUG */

#if PAGE_DEBUG
    server.on("/debug", debug_info);
#endif /* PAGE_DEBUG */

    WiFi.onStationModeDisconnected(on_disconnected);

    server.on("/", handle_root);
    server.on("/restart", restart);
    server.on("/config", redirect_to_config);
    server.on("/globals", receive_globals);
    server.on("/profile", receive_profile);
    server.on("/profile_n", change_profile);
    server.on("/color", receive_color);
    server.on("/api", send_json);
    server.on("/update", manual_update_check);
    server.on("/apply_update", apply_update);

    Udp.begin(8888);
    server.begin();

    user_init();
}

void loop()
{
    server.handleClient();
    handleUdp();

    if(flags & FLAG_NEW_FRAME)
    {
        if(auto_increment && frame && frame % auto_increment == 0 && globals.flags & GLOBALS_FLAG_ENABLED && globals.profile_count > 1)
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

        if(globals.flags & GLOBALS_FLAG_ENABLED || flags & FLAG_MANUAL_COLOR)
        {
            device_profile &device = current_profile.devices[0];
            digital_effect((effect) device.effect, strip.getPixels(), LED_COUNT, 0, frame, frames, device.args,
                           device.colors, device.color_count, device.color_cycles);

            convert_bufs();

            strip.show();
            flags &= ~FLAG_NEW_FRAME;
        }
        else
        {
            strip.setBrightness(0);
            strip.show();
        }
    }
}
