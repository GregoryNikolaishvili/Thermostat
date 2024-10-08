#ifndef _HATARGETT_H
#define _HATARGETT_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HATargetTemperatureX : public HANumber
{
public:
    HATargetTemperatureX(const char *uniqueId, const char *name, int16_t eepromAddress);
private:
    int16_t _eepromAddress;

    float readFloatFromEEPROM();
    void writeFloatToEEPROM(float value);

    static void onNumberCommand(HANumeric number, HANumber *sender);
};

#endif
