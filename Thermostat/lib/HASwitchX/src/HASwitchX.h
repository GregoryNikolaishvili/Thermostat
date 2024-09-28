#ifndef _HASWITCHX_H
#define _HASWITCHX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HASwitchX : public HASwitch
{
public:
    HASwitchX(const char *uniqueId, const char *name, byte pinId, bool relayIsOnWhenLow, bool isInverted);
    
    bool isTurnedOn();
    void setOnOff(bool on);
    void setDefaultState();

    inline byte getPin() const
    {
        return _pinId;
    }

    inline bool isRelayIsOnWhenLow() const
    {
        return _relayIsOnWhenLow;
    }

private:
    byte _pinId;
    bool _relayIsOnWhenLow;
    bool _isInverted;
    bool _mask;

    static void onRelayCommand(bool state, HASwitch *sender);
};

#endif
