#include "memory.h"
#include <EEPROM.h>
#include <HardwareSerial.h>

void loadProfile(profile *p, uint8_t n)
{
    for(uint8_t t = 0; t < PROFILE_SIZE; t++)
    {
        *((uint8_t *) p + t) = EEPROM.read(PROFILE_ADDRESS(n) + t);
        Serial.println(String(PROFILE_ADDRESS(n) + t) + ":" + String(*((uint8_t *) &p + t)));
    }
}

void saveProfile(profile *p, uint8_t n)
{
    for(uint8_t t = 0; t < PROFILE_SIZE; t++)
    {
        EEPROM.write(PROFILE_ADDRESS(n) + t, *((uint8_t *) p + t));
    }
    EEPROM.commit();
}

void loadGlobals(global_settings *globals)
{
    for(uint8_t t = 0; t < GLOBALS_SIZE; t++)
    {
        *((uint8_t *) globals + t) = EEPROM.read(GLOBALS_ADDRESS + t);
    }
}

void saveGlobals(global_settings *globals)
{
    for(uint8_t t = 0; t < GLOBALS_SIZE; t++)
    {
        EEPROM.write(GLOBALS_ADDRESS + t, *((uint8_t *) globals + t));
    }
    EEPROM.commit();
}