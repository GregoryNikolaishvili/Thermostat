#include "HATargetTemperaturePoolX.h"

HATargetTemperaturePoolX::HATargetTemperaturePoolX(const char *uniqueId, const char *name, int16_t eepromAddress)
    : HASettingX(uniqueId, name, 30, 28, 32, eepromAddress)
{
  setStep(1);
  setIcon("mdi:pool-thermometer");
  setMode(HANumber::ModeBox);
}
