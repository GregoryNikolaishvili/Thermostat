#include "HASwitchX.h"

HASwitchX::HASwitchX(const char *uniqueId, const char *name, byte pinId, bool relayIsOnWhenLow, bool isInverted)
    : HASwitch(uniqueId), _pinId(pinId), _relayIsOnWhenLow(relayIsOnWhenLow), _isInverted(isInverted)
{
  _mask = relayIsOnWhenLow ^ isInverted;

  setCurrentState(isInverted);
  setName(name);
  setIcon("mdi:pipe-valve");
  setDeviceClass("switch");

  digitalWrite(pinId, relayIsOnWhenLow);
  pinMode(pinId, OUTPUT);

  onCommand(onRelayCommand);
}

bool HASwitchX::isTurnedOn()
{
  return digitalRead(_pinId) == _mask;
}

void HASwitchX::setOnOff(bool turnOn)
{
  digitalWrite(_pinId, turnOn ^ _mask);
  setState(turnOn); // report state back to Home Assistant
}

void HASwitchX::setDefaultState()
{
  digitalWrite(_pinId, _relayIsOnWhenLow);
  setState(_isInverted); // report state back to Home Assistant
}

void HASwitchX::onRelayCommand(bool state, HASwitch *sender)
{
  HASwitchX *relay = static_cast<HASwitchX *>(sender);
  relay->setOnOff(state);
}
