#include "HASwitchX.h"

static void onRelayCommand(bool state, HASwitch *sender);

HASwitchX::HASwitchX(const char *uniqueId, const char *name, byte pinId, bool isInverted)
    : HASwitch(uniqueId), _pinId(pinId), _isInverted(isInverted)
{
  setCurrentState(!isInverted);
  setName(name);
  setIcon("mdi:pipe-valve");
  setDeviceClass("switch");

  digitalWrite(pinId, LOW);
  pinMode(pinId, OUTPUT);

  onCommand(onRelayCommand);
}

bool HASwitchX::isTurnedOn()
{
  if (_isInverted)
    return digitalRead(_pinId) == LOW;

  return digitalRead(_pinId) == HIGH;
}

void HASwitchX::setOnOff(bool state)
{
  if (_isInverted)
    digitalWrite(_pinId, !state);
  else
    digitalWrite(_pinId, state);
  setState(state); // report state back to Home Assistant
}

void HASwitchX::setDefaultState()
{
  digitalWrite(_pinId, LOW);
  setState(!_isInverted); // report state back to Home Assistant
}

static void onRelayCommand(bool state, HASwitch *sender)
{
  HASwitchX *relay = (HASwitchX *)sender;
  relay->setOnOff(state);
}
