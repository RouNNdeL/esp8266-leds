#include <stdint.h>
#include "config.h"
#include "color_utils.h"

#ifndef WIFICONTROLLER_EEPROM_H
#define WIFICONTROLLER_EEPROM_H

typedef struct
{
    uint8_t effect;
    uint8_t color_count;
    uint8_t timing[TIME_COUNT];
    uint8_t args[ARG_COUNT];
    uint8_t colors[COLOR_COUNT * 3];
} __attribute__((packed)) device_profile;

typedef struct
{
    uint8_t brightness[DEVICE_COUNT];
    uint8_t flags[DEVICE_COUNT];
    uint8_t color[3 * DEVICE_COUNT];
    uint8_t current_device_profile[DEVICE_COUNT];
    uint8_t profile_count;
    uint8_t current_profile;
    uint8_t auto_increment;
    int8_t profiles[PROFILE_COUNT][DEVICE_COUNT];
    uint8_t profile_flags[PROFILE_COUNT];
} __attribute__((packed)) global_settings;

void load_globals(global_settings *globals);

void load_device(device_profile *p, uint8_t d, uint8_t n);

void save_globals(global_settings *globals);

void save_profile(device_profile *p, uint8_t d, uint8_t n);

uint8_t get_reset_count();

uint8_t set_reset_count(uint8_t count);

uint8_t increase_reset_count();

#define GLOBALS_FLAG_ENABLED (1 << 0)
#define GLOBALS_FLAG_STATUSES (1 << 1)
#define GLOBALS_FLAG_EFFECTS (1 << 2)
#define GLOBALS_FLAG_TRANSITION (1 << 7)
#define GLOBALS_FLAG_PROFILE_UPDATED (1 << 7)

#define GLOBALS_SIZE sizeof(global_settings)
#define DEVICE_SIZE sizeof(device_profile)

#define GLOBALS_ADDRESS 0
#define DEVICE_ADDRESS(d, n) GLOBALS_ADDRESS + GLOBALS_SIZE + DEVICE_SIZE * (n + DEVICE_PROFILE_COUNT  * d)
#define RESET_ADDRESS DEVICE_ADDRESS(DEVICE_COUNT, DEVICE_PROFILE_COUNT)

#endif //WIFICONTROLLER_EEPROM_H
