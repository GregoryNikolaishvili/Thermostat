#include "HASettingX.h"
#include <EEPROM.h>

HASettingX::HASettingX(const char *uniqueId, const char *name, int16_t defaultValue, int16_t min, int16_t max, int16_t eepromAddress)
    : HANumber(uniqueId, HANumber::PrecisionP0), _defaultValue(defaultValue), _eepromAddress(eepromAddress)
{
  setMin(min);
  setMax(max);

  // Read value from EEPROM
  int16_t storedValue = readIntFromEEPROM(_eepromAddress);

  Serial.print(name);
  Serial.print(" ");
  Serial.println(storedValue);

  // Check if the stored value is within valid range
  if (storedValue >= min && storedValue <= max)
  {
    setCurrentState(storedValue);
  }
  else
  {
    // Use default value and write it to EEPROM
    setCurrentState(defaultValue);
    writeIntToEEPROM(_eepromAddress, defaultValue);

    Serial.print("Default value saved: ");
    Serial.print(defaultValue);
    Serial.print("Rereading: ");
    storedValue = readIntFromEEPROM(_eepromAddress);
    Serial.println(storedValue);
  }

  setName(name);
  setStep(1);
  setMode(HANumber::ModeBox);
  setIcon("mdi:thermometer-lines");
  setUnitOfMeasurement("°C");
  setDeviceClass("temperature");

  onCommand(HASettingX::onNumberCommand);
}

int16_t HASettingX::getSettingValue()
{
  return getCurrentState().toInt16();
}

int16_t HASettingX::readIntFromEEPROM(int16_t address)
{
  int16_t value = 0;
  EEPROM.get(address, value);
  return value;
}

void HASettingX::writeIntToEEPROM(int16_t address, int16_t value)
{
  EEPROM.put(address, value);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit(); // Necessary on ESP devices
#endif
}

void HASettingX::onNumberCommand(HANumeric number, HANumber *sender)
{
  HASettingX *setting = static_cast<HASettingX *>(sender);

  int16_t intValue;

  if (!number.isSet())
  {
    Serial.println(F("Reset setting command received"));

    // Handle the reset command
    intValue = setting->_defaultValue;
  }
  else
  {
    intValue = number.toInt16();

    Serial.print(F("Setting received: "));
    Serial.println(intValue);
  }

  // Only write to EEPROM if the value has changed
  if (intValue != setting->getSettingValue())
  {
    setting->writeIntToEEPROM(setting->_eepromAddress, intValue);
  }

  // Report the new state back to Home Assistant
  sender->setState(intValue);
}
