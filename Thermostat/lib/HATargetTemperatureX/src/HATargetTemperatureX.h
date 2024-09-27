#ifndef _HATARGETT_h
#define _HATARGETT_h

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HATargetTemperatureX : public HANumber
{
public:
    HATargetTemperatureX(const char *uniqueId, const char *name);
    float getTargetTemperature();
};

#endif
