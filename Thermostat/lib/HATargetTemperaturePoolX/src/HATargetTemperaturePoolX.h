#ifndef _HATARGETTPOOL_H
#define _HATARGETTPOOL_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>
#include <HASettingX.h>

class HATargetTemperaturePoolX : public HASettingX
{
public:
    HATargetTemperaturePoolX(const char *uniqueId, const char *name, int16_t eepromAddress);
};

#endif
