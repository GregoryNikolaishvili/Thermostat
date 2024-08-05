#ifndef _HASWITCHX_h
#define _HASWITCHX_h

#include <ArduinoHA.h>

class HASwitchX : public HASwitch
{
public:
    HASwitchX(const char *uniqueId, const char *name, byte pinId, bool isInverted);

    inline byte getPin() const
    {
        return _pinId;
    }

    inline bool IsInverted() const
    {
        return _isInverted;
    }

private:
    byte _pinId;
    bool _isInverted;

    /// The name that was set using setName method. It can be nullptr.
    const char *_name;
};

#endif
