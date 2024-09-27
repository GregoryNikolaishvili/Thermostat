#ifndef _HASETTING_h
#define _HASETTING_h

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HASettingX : public HANumber
{
public:
    HASettingX(const char *uniqueId, const char *name, int initialValue, int min, int max);

    int getSettingValue();
    void setInitialValue();

private:
    int _initialValue;
};

#endif
