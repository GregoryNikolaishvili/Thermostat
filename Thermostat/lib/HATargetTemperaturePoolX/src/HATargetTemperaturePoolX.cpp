#include "HATargetTemperaturePoolX.h"

HATargetTemperaturePoolX::HATargetTemperaturePoolX(const char *uniqueId, const char *name)
    : HATargetTemperatureX(uniqueId, name)
{
  setMin(28);
  setMax(32);
  setStep(1);
  setIcon("mdi:pool-thermometer");
}
