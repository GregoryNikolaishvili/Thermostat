#include "HATargetTemperatureX.h"
#include <EEPROM.h>

HATargetTemperatureX::HATargetTemperatureX(const char *uniqueId, const char *name, int16_t eepromAddress)
    : HANumber(uniqueId, HANumber::PrecisionP1), _eepromAddress(eepromAddress)
{
  setMin(16.0f);
  setMax(30.0f);
  setStep(0.1f);

  // Read value from EEPROM
  float storedValue = readFloatFromEEPROM(_eepromAddress);

  // Check if the stored value is within valid range
  if (storedValue >= 16.0f && storedValue <= 30.0f)
  {
    setCurrentState(storedValue);
  }
  else
  {
    // Use default value and write it to EEPROM
    setCurrentState(22.0f);
    writeFloatToEEPROM(_eepromAddress, 22.0f);
  }

  setName(name);
  setIcon("mdi:thermometer-check");
  setUnitOfMeasurement("°C");
  setDeviceClass("temperature");

  onCommand(HATargetTemperatureX::onNumberCommand);
}

float HATargetTemperatureX::readFloatFromEEPROM(int16_t address)
{
  float value = 0.0f;
  EEPROM.get(address, value);
  return value;
}

void HATargetTemperatureX::writeFloatToEEPROM(int16_t address, float value)
{
  EEPROM.put(address, value);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit(); // Necessary on ESP devices
#endif
}

void HATargetTemperatureX::onNumberCommand(HANumeric number, HANumber *sender)
{
  HATargetTemperatureX *setting = static_cast<HATargetTemperatureX *>(sender);

  float floatValue;
  if (!number.isSet())
  {
    Serial.println(F("Reset Target temperature command received"));

    // Handle the reset command
    floatValue = 22.0f;
  }
  else
  {
    floatValue = number.toFloat();

    Serial.print(F("Target temperature received: "));
    Serial.println(floatValue);
  }

  // Only write to EEPROM if the value has changed
  if (floatValue != setting->getCurrentState().toFloat())
  {
    setting->writeFloatToEEPROM(setting->_eepromAddress, floatValue);
  }

  // Report the new state back to Home Assistant
  sender->setState(floatValue);
}
