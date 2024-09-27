#ifndef _HATARGETTPOOL_h
#define _HATARGETTPOOL_h

#include <ProjectDefines.h>
#include <ArduinoHA.h>
#include <HATargetTemperatureX.h>

class HATargetTemperaturePoolX : public HATargetTemperatureX
{
public:
    HATargetTemperaturePoolX(const char *uniqueId, const char *name);
};

#endif
