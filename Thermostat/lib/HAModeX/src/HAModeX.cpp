#include "HAModeX.h"
#include <EEPROM.h>

HAModeX::HAModeX(int16_t eepromAddress)
    : HASelect("mode"), _eepromAddress(eepromAddress)
{
  int16_t storedValue = readIntFromEEPROM();
  Serial.print(F("Mode: "));
	Serial.println(storedValue);

  if (storedValue >= 0 && storedValue <= 3)
  {
    setCurrentState(storedValue);
  }
  else
  {
    setCurrentState(1);
    writeIntToEEPROM(1);
  }

  setName("Thermostat mode");
  setIcon("mdi:auto-mode");
  setOptions("Off;Winter;Summer;Summer & Pool");
}

int16_t HAModeX::readIntFromEEPROM()
{
  int16_t value = 0;
  EEPROM.get(_eepromAddress, value);
  return value;
}

void HAModeX::writeIntToEEPROM(int16_t value)
{
  EEPROM.put(_eepromAddress, value);
#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit(); // Necessary on ESP devices
#endif
}