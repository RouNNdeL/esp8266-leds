//
// Created by Krzysiek on 2018-05-03.
//

#ifndef WIFICONTROLLER_CONFIG_H
#define WIFICONTROLLER_CONFIG_H


#define VERSION_CODE 22
#define VERSION_NAME "1.0"
#define BUILD_DATE (String(__TIME__)+"@" + __DATE__)
#define REQUEST_RETIRES 10
#define RECOVERY_ATTEMPTS 5

#define FPS 100
#define TRANSITION_FRAMES 50
#define TRANSITION_QUICK_FRAMES 35

#if TRANSITION_FRAMES <= 255 && TRANSITION_QUICK_FRAMES <= 255
typedef uint8_t transition_t;
#else
typedef uint16_t transition_t;
#endif

#ifndef VIRTUAL_DEVICES
#define VIRTUAL_DEVICES {60}
#endif /* VIRTUAL_DEVICES */

#ifndef LED_PIN
#define LED_PIN 4
#endif /* LED_PIN */

#ifndef LED_COUNT
#define LED_COUNT 60
#endif /* LED_COUNT */


#ifndef AP_NAME
#define AP_NAME "LED Controller"
#endif /* AP_NAME */

#ifndef DEVICE_ID
#define DEVICE_ID "iot_0"
#endif /* DEVICE_ID */

#ifndef CONFIG_PAGE
#define CONFIG_PAGE "http://led/"
#endif /* CONFIG_PAGE */

#define HTTP_SERVER_HOST "home.zdul.xyz"
#define HTTP_SERVER_PORT_HTTP 80
#define HTTP_SERVER_PORT_HTTPS 443
#define HTTP_SERVER_HTTPS_FINGERPRINT "16 32 09 61 08 56 fa 41 c2 ad 0c 25 08 2a 06 1d 67 1d 55 fb"

#define HTTP_UPDATE_HTTPS 1
#define HTTP_UPDATE_URL "/api/esp_update.php"

#define HTTP_STATE_HTTPS 1
#define HTTP_STATE_URL "/api/local/esp_report_state.php"

#define HTTP_HALT_HTTPS 1
#define HTTP_HALT_URL "/api/local/esp_report_halt.php"

#define UDP_DISCOVERY_MSG "ROUNDEL_IOT_DISCOVERY"
#define UDP_DISCOVERY_RESPONSE "ROUNDEL_IOT_RESPONSE"

#ifndef PROFILE_COUNT
#define PROFILE_COUNT 24
#endif /* PROFILE_COUNT */

#ifndef DEVICE_PROFILE_COUNT
#define DEVICE_PROFILE_COUNT 24
#endif /* DEVICE_PROFILE_COUNT */

#ifndef DEVICE_COUNT
#define DEVICE_COUNT 1
#endif /* DEVICE_COUNT */

#define COLOR_COUNT 16
#define ARG_COUNT 6
#define TIME_COUNT 6

#define SCALE8_C 1
#define FASTLED_SCALE8_FIXED 1

#ifndef PAGE_DEBUG
#define PAGE_DEBUG 1
#endif /* PAGE_DEBUG */

#endif //WIFICONTROLLER_CONFIG_H
