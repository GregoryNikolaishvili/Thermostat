#ifndef _HALIGHTRELAY_h
#define _HALIGHTRELAY_h

#include <ArduinoHA.h>

class HASwitchX : public HASwitch {
public:
  HASwitchX::HASwitchX(const char* uniqueId, const char* name, byte pinId);
  inline byte getPin() const {
    return _pinId;
  }

private:
  byte _pinId;

  /// The name that was set using setName method. It can be nullptr.
  const char* _name;
};

#endif
