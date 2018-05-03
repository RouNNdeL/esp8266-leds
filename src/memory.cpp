#include "memory.h"
#include <EEPROM.h>
#include <HardwareSerial.h>

void load_profile(profile *p, uint8_t n)
{
    for(uint8_t t = 0; t < PROFILE_SIZE; t++)
    {
        *((uint8_t *) p + t) = EEPROM.read(PROFILE_ADDRESS(n) + t);
    }
}

void save_profile(profile *p, uint8_t n)
{
    for(uint8_t t = 0; t < PROFILE_SIZE; t++)
    {
        EEPROM.write(PROFILE_ADDRESS(n) + t, *((uint8_t *) p + t));
    }
    EEPROM.commit();
}

void load_globals(global_settings *globals)
{
    for(uint8_t t = 0; t < GLOBALS_SIZE; t++)
    {
        *((uint8_t *) globals + t) = EEPROM.read(GLOBALS_ADDRESS + t);
    }
}

void save_globals(global_settings *globals)
{
    for(uint8_t t = 0; t < GLOBALS_SIZE; t++)
    {
        EEPROM.write(GLOBALS_ADDRESS + t, *((uint8_t *) globals + t));
    }
    EEPROM.commit();
}