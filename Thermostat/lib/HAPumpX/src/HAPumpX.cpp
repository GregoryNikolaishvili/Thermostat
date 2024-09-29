#include "HaPumpX.h"

HAPumpX::HAPumpX(const char *uniqueId, const char *name, byte pinId)
    : HASwitch(uniqueId), _pinId(pinId)
{
  setCurrentState(LOW);
  setName(name);
  setIcon("mdi:pipe-valve");
  setDeviceClass("switch");

  digitalWrite(pinId, HIGH);
  pinMode(pinId, OUTPUT);

  onCommand(onRelayCommand);
}

bool HAPumpX::isTurnedOn()
{
  return digitalRead(_pinId) == LOW;
}

void HAPumpX::setOnOff(bool turnOn)
{
  digitalWrite(_pinId, turnOn ? LOW : HIGH);
  setState(turnOn); // report state back to Home Assistant
}
void HAPumpX::onRelayCommand(bool state, HASwitch *sender)
{
  HAPumpX *relay = static_cast<HAPumpX *>(sender);
  relay->setOnOff(state);
}
