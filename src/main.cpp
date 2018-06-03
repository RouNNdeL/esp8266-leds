#include <Arduino.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoOTA.h>

extern "C" {
#include "user_interface.h"
#include "color_utils.h"
}

#include "config.h"
#include "memory.h"

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP8266WebServer server(80);

#define FLAG_NEW_FRAME (1 << 0)
#define FLAG_PROFILE_UPDATED (1 << 1)
#define FLAG_TRANSITION (1 << 2)

os_timer_t frameTimer;
volatile uint32_t frame;
volatile uint8_t flags;

led_count_t virtual_devices[DEVICE_COUNT] = VIRTUAL_DEVICES;

global_settings globals;
#define increment_profile() globals.n_profile = (globals.n_profile+1)%globals.profile_count

/* The first 3 bytes are the current color, the last 3 bytes are the old color to transition from */
uint8_t color_converted[6 * DEVICE_COUNT] = {};
volatile transition_t transition_frame;
String requestId = "";

#define refresh_profile() load_profile(&current_profile, globals.profile_order[globals.n_profile]); \
convert_all_frames()

profile current_profile;
uint16_t frames[DEVICE_COUNT][TIME_COUNT];
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

void convert_all_frames()
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        convert_to_frames(frames[i], current_profile.devices[i].timing);
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

void convert_color()
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        uint8_t index = i * 6;
        /* Copy the now old color to its place (3 bytes after the new) */
        memcpy(color_converted + index + 3, color_converted + index, 3);
        set_color_manual(color_converted, color_brightness(
                (globals.flags[i] & GLOBALS_FLAG_ENABLED) ? globals.brightness[i] : 0,
                color_from_buf(globals.color + index)
        ));
    }

    transition_frame = 0;
    flags |= FLAG_TRANSITION;
}

void timerCallback(void *pArg)
{
    flags |= FLAG_NEW_FRAME;
    frame++;
    if(flags & FLAG_TRANSITION) transition_frame++;
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
    convert_color();
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
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        if(!(globals.flags[i] & GLOBALS_FLAG_STATUSES))
            return;
        strip.setBrightness(UINT8_MAX);
        for(led_count_t j = 0; j < virtual_devices[i]; ++j)
        {
            if(j % 4)
                strip.setPixelColor(j, color_brightness(12, r, g, b));
            else
                strip.setPixelColor(j, r, g, b);
        }
        strip.show();
    }
}

int checkUpdate()
{
    int code;
    uint8_t tries = 0;
    do
    {
        tries++;
        HTTPClient http;

#if HTTP_UPDATE_HTTPS
        http.begin(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTPS, String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID,
                   HTTP_SERVER_HTTPS_FINGERPRINT);
#else
        http.begin(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTP, String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID);
#endif /* HTTP_UPDATE_HTTPS */
        http.useHTTP10(true);
        http.setTimeout(2500);
        http.setUserAgent(F("ESP8266-http-Update"));
        http.addHeader(F("x-Request-Attempts"), String(tries));
        http.addHeader(F("x-ESP8266-STA-MAC"), WiFi.macAddress());
        http.addHeader(F("x-ESP8266-AP-MAC"), WiFi.softAPmacAddress());
        http.addHeader(F("x-ESP8266-free-space"), String(ESP.getFreeSketchSpace()));
        http.addHeader(F("x-ESP8266-sketch-size"), String(ESP.getSketchSize()));
        http.addHeader(F("x-ESP8266-sketch-md5"), String(ESP.getSketchMD5()));
        http.addHeader(F("x-ESP8266-chip-size"), String(ESP.getFlashChipRealSize()));
        http.addHeader(F("x-ESP8266-sdk-version"), ESP.getSdkVersion());
        http.addHeader(F("x-ESP8266-mode"), F("check"));
        http.addHeader(F("x-ESP8266-version"), String(VERSION_CODE));

        code = http.GET();
        http.end();
    }
    while((code == -1 || code == -11) && tries < REQUEST_RETIRES);
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
    HTTPUpdateResult ret = ESPhttpUpdate.update(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTPS,
                                                String(HTTP_UPDATE_URL) + "?device_id=" + DEVICE_ID,
                                                String(VERSION_CODE), HTTP_SERVER_HTTPS_FINGERPRINT);
#else
    HTTPUpdateResult ret = ESPhttpUpdate.update(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTP,
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

String getDeviceJson()
{
    String profile_array = "[";
    for(uint8_t i = 0; i < globals.profile_count; ++i)
    {
        if(i) profile_array += ",";
        profile_array += String(globals.profile_order[i]);
    }
    profile_array += "]";

    String brightness_array = "[";
    String flags_array = "[";
    String color_array = "[";
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        if(i) brightness_array += ",";
        if(i) flags_array += ",";
        if(i) color_array += ",";

        brightness_array += String(globals.brightness[i]);
        flags_array += String(globals.flags[i]);
        color_array += String((uint32_t) globals.color[0] << 16 | (uint16_t) globals.color[1] << 8 | globals.color[2]);
    }
    brightness_array += "]";
    flags_array += "]";
    color_array += "]";

    String content = R"({"ap_name":")" + String(AP_NAME) +
                     R"(","version_code":)" + String(VERSION_CODE) +
                     R"(,"version_name":")" + String(VERSION_NAME) +
                     R"(","device_id":")" + DEVICE_ID +
                     "\",\"auto_increment\":" + String(auto_increment / FPS) +
                     ",\"current_profile\":" + String(globals.n_profile) +
                     ",\"color\":" + color_array +
                     ",\"flags\":" + flags_array +
                     ",\"brightness\":" + brightness_array +
                     ",\"profiles\": " + profile_array + "}";
    return content;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

int sendDeviceState()
{
    int code;
    uint8_t tries = 0;
    do
    {
        tries++;
        HTTPClient http;
#if HTTP_STATE_HTTPS
        http.begin(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTPS, String(HTTP_STATE_URL), HTTP_SERVER_HTTPS_FINGERPRINT);
#else
        http.begin(HTTP_SERVER_HOST, HTTP_SERVER_PORT_HTTP, String(HTTP_STATE_URL));
#endif /* HTTP_UPDATE_HTTPS */

        http.setUserAgent(F("ESP8266"));
        http.addHeader(F("Content-Type"), "application/json");
        http.addHeader(F("x-Request-Attempts"), String(tries));

        if(requestId.length() > 0 && requestId[0] != 0x00)
            http.addHeader(F("x-Request-Id"), requestId);


        code = http.POST(getDeviceJson());
        http.end();
    }
    while((code == -1 || code == -11) && tries < REQUEST_RETIRES);
    return code;
}

#pragma clang diagnostic pop

#if PAGE_DEBUG

void ICACHE_FLASH_ATTR debug_info()
{
    String r = String(globals.color[0], 16);
    String g = String(globals.color[1], 16);
    String b = String(globals.color[2], 16);
    r = r.length() == 1 ? "0" + r : r;
    g = g.length() == 1 ? "0" + g : g;
    b = b.length() == 1 ? "0" + b : b;

    String flags_array = "[";
    String brightness_array = "[";
    for(uint8_t i = 0; i < DEVICE_COUNT; i++)
    {
        if(i) flags_array += ",";
        if(i) brightness_array += ",";

        flags_array += String(globals.flags[i]);
        brightness_array += String(globals.brightness[i]);
    }
    flags_array += "]";
    brightness_array += "]";


    String content = "<h2>" + String(AP_NAME) + "</h2>";
    content += "<p>config_url: <a href=\"" + String(CONFIG_PAGE) + "\">" + CONFIG_PAGE + "</a></p>";
    content += "<p>version_code: <b>" + String(VERSION_CODE) + "</b></p>";
    content += "<p>version_name: <b>" + String(VERSION_NAME) + "</b></p>";
    content += "<p>build_date: <b>" + BUILD_DATE + "</b></p>";
    content += "<p>reset_info: <b>" + ESP.getResetReason() + "</b></p>";
    content += "<p>device_id: <b>" + String(DEVICE_ID) + "</b></p>";
    content += "<p>device_flags: <b>" + String(flags) + "</b></p>";
    content += "<p>device_frame: <b>" + String(frame) + "</b></p>";
    content += "<p>device_transition_frame: <b>" + String(transition_frame) + "</b></p>";
    content += "<p>flags: <b>" + flags_array + "</b></p>";
    content += "<p>color: <b>#" + r + g + b + "</b></p>";
    content += "<p>brightness: <b>" + brightness_array + "</b></p>";
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
    content += "<p>color_converted: <b>[";
    for(uint8_t i = 0; i < sizeof(color_converted); i++)
    {
        if(i) content += ",";
        content += String(color_converted[i]);
    }
    content += "]</b></p>";
    server.send(200, "text/html", content);
}

#endif /* PAGE_DEBUG */

void ICACHE_FLASH_ATTR receive_globals()
{
    if(server.hasArg("plain") &&
       server.method() == HTTP_PUT &&
       (server.arg("plain").length() == GLOBALS_SIZE * 2 ||
        (server.arg("plain").length() < GLOBALS_SIZE &&
         server.arg("plain").end()[-1] == '*')))
    {
        uint8_t bytes[GLOBALS_SIZE];
        auto c = server.arg("plain");
        for(uint8_t i = 0; i < GLOBALS_SIZE; ++i)
        {
            if(c[i * 2] == '*' || c[i * 2 + 1] == '*')
            {
                memcpy(bytes + i, ((uint8_t *) &globals) + i, GLOBALS_SIZE - i);
                break;
            }
            if(c[i * 2] == '?' && c[i * 2 + 1] == '?')
            {
                bytes[i] = ((uint8_t *) &globals)[i];
            }
            else
            {
                bytes[i] = char2int(c[i * 2]) * 16 + char2int(c[i * 2 + 1]);
            }
        }
        uint8_t previous_profile = globals.profile_order[globals.n_profile];
        uint8_t previous_auto_increment = globals.auto_increment;
        memcpy(&globals, bytes, GLOBALS_SIZE);
        if(previous_profile != globals.profile_order[globals.n_profile])
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
        requestId = server.header("x-Request-Id");
        convert_color();
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
            convert_all_frames();
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

void ICACHE_FLASH_ATTR handle_root()
{
    // TODO: Add separate pages for each virtual device
    String r = String(globals.color[0], 16);
    String g = String(globals.color[1], 16);
    String b = String(globals.color[2], 16);
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
        content += "<option value=\"" + String(i) + "\" " + selected + ">" + String(globals.profile_order[i]) +
                   "</option>";
    }
    content += R"(</select><p><a href="/restart">Restart device</a></p><p><a href="/update">Check for updates</a></p> <script>document.getElementById("a").addEventListener("change",function(e){var t=new XMLHttpRequest;t.open("PUT","/globals",!0),t.send("????"+e.target.value.substring(1)+"*")}),document.getElementById("b").addEventListener("change",function(e){var t=new XMLHttpRequest;t.open("PUT","/globals",!0),t.send("??????????"+("0"+(16).toString(e.target.value)).slice(-2))+"*";});</script> </body></html>)";
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
    server.send(200, "application/json", getDeviceJson());
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

void recover()
{
    EEPROM.begin(SPI_FLASH_SEC_SIZE);

    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        globals.brightness[i] = 0xff;
        globals.flags[i] = GLOBALS_FLAG_ENABLED | GLOBALS_FLAG_STATUSES;

        current_profile.devices[i].effect = PIECES;
        current_profile.devices[i].color_count = 2;
        current_profile.devices[i].color_cycles = 1;
        current_profile.devices[i].timing[TIME_OFF] = 0;
        current_profile.devices[i].timing[TIME_FADEIN] = 0;
        current_profile.devices[i].timing[TIME_ON] = 60;
        current_profile.devices[i].timing[TIME_FADEOUT] = 30;
        current_profile.devices[i].timing[TIME_ROTATION] = 90;
        current_profile.devices[i].timing[TIME_DELAY] = 0;
        current_profile.devices[i].args[ARG_BIT_PACK] = DIRECTION | SMOOTH;
        current_profile.devices[i].args[ARG_PIECES_PIECE_COUNT] = 6;
        current_profile.devices[i].args[ARG_PIECES_COLOR_COUNT] = 2;
        set_color_manual(current_profile.devices[i].colors, grb(COLOR_RED));
        set_color_manual(current_profile.devices[i].colors + 3, grb(COLOR_BLUE));
    }

    globals.auto_increment = 0;
    globals.profile_order[0] = 0;
    globals.profile_count = 1;
    globals.n_profile = 0;

    convert_all_frames();

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

    if(checkUpdate() == HTTP_CODE_OK) update(1);

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
    server.on("/api", send_json);
    server.on("/update", manual_update_check);
    server.on("/apply_update", apply_update);

    const char *headers[] = {"x-Request-Id"};
    size_t headers_size = sizeof(headers) / sizeof(char *);
    server.collectHeaders(headers, headers_size);

    server.begin();
    ArduinoOTA.begin();

    user_init();
}

void loop()
{
    /* We do not want the transition to lag, so we don't handle the client */
    if(!(flags & FLAG_TRANSITION))
    {
        server.handleClient();
    }

    ArduinoOTA.handle();

    if(flags & FLAG_NEW_FRAME)
    {
        flags &= ~FLAG_NEW_FRAME;
        uint8 enabled = 0;

        uint16_t virtual_led_offset = 0;
        for(uint8_t d = 0; d < DEVICE_COUNT; ++d)
        {
            uint8_t *p = strip.getPixels() + virtual_led_offset * 3;
            if(globals.flags[d] & GLOBALS_FLAG_ENABLED | flags & FLAG_TRANSITION)
            {
                enabled = 1;
                if(globals.flags[d] & GLOBALS_FLAG_EFFECTS)
                {

                    device_profile &device = current_profile.devices[d];
                    digital_effect((effect) device.effect, p, virtual_devices[d], 0, frame + frames[d][TIME_DELAY],
                                   frames[d], device.args, device.colors, device.color_count, device.color_cycles);


                    for(uint8_t i = 0; i < virtual_devices[d]; ++i)
                    {
                        uint8_t index = i * 3;
                        set_color_manual(p + index, color_brightness(globals.brightness[d], color_from_buf(p + index)));
                        p[index] = actual_brightness(p[index]);
                        p[index + 1] = actual_brightness(p[index + 1]);
                        p[index + 2] = actual_brightness(p[index + 2]);
                    }

                    strip.show();
                }
                else
                {
                    uint8_t index = d * 6;
                    uint8_t color[3];
                    cross_fade(color, color_converted + index, 3, 0, transition_frame * UINT8_MAX / TRANSITION_FRAMES);
                    color[index] = actual_brightness(color[index]);
                    color[index + 1] = actual_brightness(color[index + 1]);
                    color[index + 2] = actual_brightness(color[index + 2]);
                    for(led_count_t i = 0; i < virtual_devices[d]; ++i)
                    {
                        set_color_manual(p + i * 3, grb(color_from_buf(color)));
                    }
                    strip.show();

                    if(transition_frame >= TRANSITION_FRAMES && flags & FLAG_TRANSITION)
                    {
                        memcpy(color_converted + index + 3, color_converted + index, 3);
                        flags &= ~FLAG_TRANSITION;
                        sendDeviceState();
                    }
                }
            }
            else
            {
                for(led_count_t i = 0; i < LED_COUNT; ++i)
                {
                    set_color_manual(p + i * 3, grb(COLOR_BLACK));
                }
                strip.show();
            }

            virtual_led_offset += virtual_devices[d];
        }

        if(auto_increment && frame && frame % auto_increment == 0 && enabled && globals.profile_count > 1)
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
    }
}
