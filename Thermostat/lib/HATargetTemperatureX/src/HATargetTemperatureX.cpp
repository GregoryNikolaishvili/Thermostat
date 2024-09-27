#include "HATargetTemperatureX.h"

static void onTargetTemperatureCommand(HANumeric number, HANumber *sender);

HATargetTemperatureX::HATargetTemperatureX(const char *uniqueId, const char *name)
    : HANumber(uniqueId, HANumber::PrecisionP1)
{
  setName(name);
  setCurrentState(22.0f);
  setMin(16);
  setMax(30);
  setStep(0.1f);
  // setMode(HANumber::ModeAuto);
  setRetain(true);
  setIcon("mdi:thermometer-check");
  setUnitOfMeasurement("°C");
  setDeviceClass("temperature");

  onCommand(onTargetTemperatureCommand);
}

float HATargetTemperatureX::getTargetTemperature()
{
  return getCurrentState().toFloat();
}

static void onTargetTemperatureCommand(HANumeric number, HANumber *sender)
{
  if (!number.isSet())
  {
    Serial.println("Reset command received");
    // the reset command was send by Home Assistant
  }
  else
  {
    // you can do whatever you want with the number as follows:
    float value = number.toFloat();

    Serial.print("Target temperature received: ");
    Serial.println(value);
  }

  sender->setState(number); // report the selected option back to the HA panel
}
