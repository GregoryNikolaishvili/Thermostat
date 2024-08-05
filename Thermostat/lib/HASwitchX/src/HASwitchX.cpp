#include "HASwitchX.h"

HASwitchX::HASwitchX(const char *uniqueId, const char *name, byte pinId, bool isInverted)
    : HASwitch(uniqueId), _pinId(pinId), _isInverted(isInverted)
{
  setState(false);
  setName(name);
  setIcon("mdi:water-pump");
  setDeviceClass("switch");

  digitalWrite(pinId, HIGH);
  pinMode(pinId, OUTPUT);
}
