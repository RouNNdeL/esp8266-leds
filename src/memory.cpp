#include "memory.h"
#include <EEPROM.h>

void loadProfile(profile p, uint8_t n)
{
    for(uint8_t t = 0; t < sizeof(profile); t++)
    {
        *((uint8_t *) &p + t) = EEPROM.read(PROFILE_ADDRESS(n) + t);
    }
}

void saveProfile(profile p, uint8_t n)
{
    for(unsigned int t = 0; t < sizeof(profile); t++)
        EEPROM.write(PROFILE_ADDRESS(n) + t, *((uint8_t *) &p + t));
}

void loadGlobals(global_settings globals)
{
    for(uint8_t t = 0; t < sizeof(global_settings); t++)
    {
        *((uint8_t *) &globals + t) = EEPROM.read(GLOBALS_ADDRESS + t);
    }
}

void saveGlobals(global_settings globals)
{
    for(uint8_t t = 0; t < sizeof(global_settings); t++)
    {
        EEPROM.write(GLOBALS_ADDRESS + t, *((uint8_t *) &globals + t));
    }
}