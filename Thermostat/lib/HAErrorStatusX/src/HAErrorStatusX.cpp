#include "HAErrorStatusX.h"
#include <EEPROM.h>

HAErrorStatusX::HAErrorStatusX()
    : HASensor("error_state", HASensor::JsonAttributesFeature), _max31865_fault(0xFF), _errors(0xFF)
{
  setName("Error state");
  setIcon("mdi:alert");
}

void HAErrorStatusX::setStatus(uint8_t max31865_fault, uint8_t errors)
{
  if (max31865_fault == _max31865_fault && errors == _errors)
    return;

  _max31865_fault = max31865_fault;
  _errors = errors;

  if (max31865_fault != 0 || errors != 0)
  {
    char json[64];
    snprintf(json, sizeof(json), "{\"max31865\":%d,\"errors\":%d}", max31865_fault, errors);

    setValue("error");
    setJsonAttributes(json);
    // Serial.println(json);
  }
  else
  {
    setValue("ok");
    setJsonAttributes(nullptr);
  }
}
