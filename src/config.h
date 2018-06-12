//
// Created by Krzysiek on 2018-05-03.
//

#ifndef WIFICONTROLLER_CONFIG_H
#define WIFICONTROLLER_CONFIG_H


#define VERSION_CODE 20
#define VERSION_NAME "0.7"
#define BUILD_DATE (String(__TIME__)+"@" + __DATE__)
#define REQUEST_RETIRES 10
#define RECOVERY_ATTEMPTS 5

#define FPS 100
#define TRANSITION_FRAMES 50

#if TRANSITION_FRAMES <= 255
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
#define HTTP_SERVER_HTTPS_FINGERPRINT "f8 0c f7 57 6c ca 1f e9 51 8f 21 7a 8f 43 0c 9c 7c 28 2c 50"

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

#ifndef DEVICE_COUNT
#define DEVICE_COUNT 1
#endif /* DEVICE_COUNT */

#define COLOR_COUNT 16
#define ARG_COUNT 5
#define TIME_COUNT 6

#define SCALE8_C 1
#define FASTLED_SCALE8_FIXED 1

#ifndef PAGE_DEBUG
#define PAGE_DEBUG 1
#endif /* PAGE_DEBUG */

#endif //WIFICONTROLLER_CONFIG_H
