#ifndef _HASETTING_H
#define _HASETTING_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HASettingX : public HANumber
{
public:
    HASettingX(const char *uniqueId, const char *name, int16_t defaultValue, int16_t min, int16_t max, int16_t eepromAddress);

    int16_t getSettingValue();

private:
    int16_t _defaultValue;
    int16_t _eepromAddress;

    int16_t readIntFromEEPROM();
    void writeIntToEEPROM(int16_t value);

    static void onNumberCommand(HANumeric number, HANumber *sender);
};

#endif
