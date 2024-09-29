#ifndef _HAHEATINGX_H
#define _HAHEATINGX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

#define VALVE_NO true
#define VALVE_NC false

class HAHeatingX : public HASwitch
{
public:
    HAHeatingX(const char *uniqueId, const char *name, byte pinId, bool normallyOpen);
    
    bool isOpen();
    void setOpenClose(bool open);
    void setDefaultState();

    inline byte getPin() const
    {
        return _pinId;
    }

    inline bool isnormallyOpen() const
    {
        return _normallyOpen;
    }

private:
    byte _pinId;
    bool _normallyOpen;

    static void onRelayCommand(bool state, HASwitch *sender);
};

#endif
