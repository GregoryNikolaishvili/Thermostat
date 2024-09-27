#include "HASwitchX.h"

static void onRelayCommand(bool state, HASwitch *sender);

HASwitchX::HASwitchX(const char *uniqueId, const char *name, byte pinId, bool isInverted)
    : HASwitch(uniqueId), _pinId(pinId), _isInverted(isInverted)
{
  setCurrentState(false);
  setName(name);
  setIcon("mdi:pipe-valve");
  setDeviceClass("switch");

  digitalWrite(pinId, HIGH);
  pinMode(pinId, OUTPUT);

  onCommand(onRelayCommand);
}

bool HASwitchX::isTurnedOn()
{
  if (_isInverted)
    return digitalRead(_pinId) == HIGH;

  return digitalRead(_pinId) == LOW;
}

void HASwitchX::setOnOff(bool state)
{
  if (_isInverted)
    digitalWrite(_pinId, state ? HIGH : LOW);
  else
    digitalWrite(_pinId, state ? LOW : HIGH);
  setState(state); // report state back to Home Assistant
}

void HASwitchX::setDefaultState()
{
  digitalWrite(_pinId, HIGH);
  setState(_isInverted); // report state back to Home Assistant
}

static void onRelayCommand(bool state, HASwitch *sender)
{
  HASwitchX *relay = (HASwitchX *)sender;
  relay->setOnOff(state);
}
