#ifndef _HAPUMPX_H
#define _HAPUMPX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HAPumpX : public HASwitch
{
public:
    HAPumpX(const char *uniqueId, const char *name, byte pinId);
    
    bool isTurnedOn();
    void setOnOff(bool on);

    inline byte getPin() const
    {
        return _pinId;
    }

private:
    byte _pinId;

    static void onRelayCommand(bool state, HASwitch *sender);
};

#endif
