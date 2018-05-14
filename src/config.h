//
// Created by Krzysiek on 2018-05-03.
//

#ifndef WIFICONTROLLER_CONFIG_H
#define WIFICONTROLLER_CONFIG_H


#define VERSION_CODE 11
#define VERSION_NAME "0.32"
#define BUILD_DATE (String(__TIME__)+"@" + __DATE__)

#define FPS 100

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
#define DEVICE_ID 0
#endif /* DEVICE_ID */

#ifndef CONFIG_PAGE
#define CONFIG_PAGE "http://led/"
#endif /* CONFIG_PAGE */

#define HTTP_UPDATE_HOST "home.zdul.xyz"
#define HTTP_UPDATE_HTTPS 1
#define HTTP_UPDATE_HTTPS_FINGERPRINT "f8 0c f7 57 6c ca 1f e9 51 8f 21 7a 8f 43 0c 9c 7c 28 2c 50"
#define HTTP_UPDATE_PORT 443
#define HTTP_UPDATE_URL "/api/esp_update.php"

#define UDP_DISCOVERY_MSG "ROUNDEL_IOT_DISCOVERY"
#define UDP_DISCOVERY_RESPONSE "ROUNDEL_IOT_RESPONSE"

#define PROFILE_COUNT 24
#define COLOR_COUNT 16
#define DEVICE_COUNT 1
#define ARG_COUNT 5
#define TIME_COUNT 6

#define SCALE8_C 1
#define FASTLED_SCALE8_FIXED 1

#ifndef PAGE_DEBUG
#define PAGE_DEBUG 1
#endif /* PAGE_DEBUG */

#endif //WIFICONTROLLER_CONFIG_H
