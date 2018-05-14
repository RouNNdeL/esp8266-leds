#include <stdint.h>
#include "config.h"
#include "color_utils.h"

#ifndef WIFICONTROLLER_EEPROM_H
#define WIFICONTROLLER_EEPROM_H

typedef struct
{
    uint8_t effect;
    uint8_t color_count;
    uint8_t color_cycles;
    uint8_t timing[TIME_COUNT];
    uint8_t args[ARG_COUNT];
    uint8_t colors[COLOR_COUNT * 3];
} __attribute__((packed)) device_profile;

typedef struct
{
    device_profile devices[DEVICE_COUNT];
    uint8_t flags;
} __attribute__((packed)) profile;

typedef struct
{
    uint8_t brightness[DEVICE_COUNT];
    uint8_t profile_count;
    uint8_t n_profile;
    uint8_t flags;
    uint8_t auto_increment;
    uint8_t color[3];
    uint8_t profile_order[PROFILE_COUNT];
} __attribute__((packed)) global_settings;

void load_globals(global_settings *globals);

void load_profile(profile *p, uint8_t n);

void save_globals(global_settings *globals);

void save_profile(profile *p, uint8_t n);

#define GLOBALS_FLAG_ENABLED (1 << 0)
#define GLOBALS_FLAG_STATUSES (1 << 1)
#define GLOBALS_FLAG_EFFECTS (1 << 2)

#define GLOBALS_SIZE sizeof(global_settings)
#define DEVICE_SIZE sizeof(device_profile)
#define PROFILE_SIZE sizeof(profile)

#define GLOBALS_ADDRESS 0
#define PROFILE_ADDRESS(n) GLOBALS_ADDRESS + GLOBALS_SIZE + PROFILE_SIZE * n

#endif //WIFICONTROLLER_EEPROM_H
