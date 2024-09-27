#include "HASettingX.h"

static void onNumberCommand(HANumeric number, HANumber *sender);

HASettingX::HASettingX(const char *uniqueId, const char *name, int initialValue, int min, int max)
    : HANumber(uniqueId, HANumber::PrecisionP0), _initialValue(initialValue)
{
  setName(name);
  setMin(min);
  setMax(max);
  setStep(1);
  // setMode(HANumber::ModeAuto);
  setIcon("mdi:thermometer-lines");
  setUnitOfMeasurement("°C");
  setDeviceClass("temperature");

  onCommand(onNumberCommand);
}

int HASettingX::getSettingValue()
{
  return getCurrentState().toInt16();
}

void HASettingX::setInitialValue()
{
  setRetain(true);
  setState(_initialValue);
}

static void onNumberCommand(HANumeric number, HANumber *sender)
{
  if (!number.isSet())
  {
    Serial.println("Reset command received");
    // the reset command was send by Home Assistant
  }
  else
  {
    // you can do whatever you want with the number as follows:
    int16_t value = number.toInt16();

    Serial.print("Setting received: ");
    Serial.println(value);
  }

  sender->setState(number); // report the selected option back to the HA panel
}
