#include "HAHeatingX.h"

HAHeatingX::HAHeatingX(const char *uniqueId, const char *name, byte pinId, bool normallyOpen)
    : HASwitch(uniqueId), _pinId(pinId), _normallyOpen(normallyOpen)
{
  setCurrentState(normallyOpen);
  setName(name);
  setIcon("mdi:pipe-valve");
  setDeviceClass("switch");

  digitalWrite(pinId, LOW);
  pinMode(pinId, OUTPUT);

  onCommand(onRelayCommand);
}

bool HAHeatingX::isOpen()
{
  return digitalRead(_pinId) == !_normallyOpen;
}

void HAHeatingX::setOpenClose(bool open)
{
  digitalWrite(_pinId, _normallyOpen);
  setState(open); // report state back to Home Assistant
}

void HAHeatingX::setDefaultState()
{
  digitalWrite(_pinId, LOW);
  setState(_normallyOpen); // report state back to Home Assistant
}

void HAHeatingX::onRelayCommand(bool state, HASwitch *sender)
{
  HAHeatingX *relay = static_cast<HAHeatingX *>(sender);
  relay->setOpenClose(state);
}
