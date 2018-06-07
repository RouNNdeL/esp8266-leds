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

uint8_t get_reset_count()
{
    return EEPROM.read(RESET_ADDRESS);
}

uint8_t set_reset_count(uint8_t count)
{
    EEPROM.write(RESET_ADDRESS, count);
}

uint8_t increase_reset_count()
{
    set_reset_count(get_reset_count()+1);
}