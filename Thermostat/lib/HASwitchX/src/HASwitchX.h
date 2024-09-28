#ifndef _HASWITCHX_H
#define _HASWITCHX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HASwitchX : public HASwitch
{
public:
    HASwitchX(const char *uniqueId, const char *name, byte pinId, bool isInverted);
    
    bool isTurnedOn();
    void setOnOff(bool on);
    void setDefaultState(const bool force = false);

    inline byte getPin() const
    {
        return _pinId;
    }

    inline bool isInverted() const
    {
        return _isInverted;
    }

private:
    byte _pinId;
    bool _isInverted;
};

#endif
