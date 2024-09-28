#include "SolarTemperatureReader.h"

#ifndef T_UNDEFINED
#define T_UNDEFINED 999
#endif

SolarTemperatureReader::SolarTemperatureReader(int8_t spi_cs)
{
  pinMode(spi_cs, OUTPUT);
  digitalWrite(spi_cs, HIGH); // Disable MAX31865

  _solarSensor = new Adafruit_MAX31865(spi_cs);

  _solarSensor->begin(MAX31865_2WIRE);
  _solarSensor->enable50Hz(true);

  _solarSensor->readRTD();
}

// Returns temperature, or T_UNDEFINED
float SolarTemperatureReader::getSolarPaneTemperature(TinyJsonBuilder &jsonBuilder)
{
  // const float RREF = 4300.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
  // const float RREF = 4265.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
  const float RREF = 4000.0; // https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code

  float Rt = _solarSensor->readRTD();
  Rt /= 32768;
  Rt *= RREF;

  // Serial.println(Rt);

  float temperature = _solarSensor->temperature(1000.0, RREF);

  // Serial.println(temperature);

  // Serial.print(F("RTD value: ")); Serial.println(lastReadSolarPanelRTD);
  // Serial.print(F("RTD Resistance = ")); Serial.println(RREF * lastReadSolarPanelRTD / 32768, 8);
  // Serial.print(F("RTD Temperature = ")); Serial.println(temperature);

  // Check and print any faults
  uint8_t fault = _solarSensor->readFault();
  if (fault)
  {
    _solarSensor->clearFault();

    String error = "Fault 0x" + String(fault, HEX);
    if (fault & MAX31865_FAULT_HIGHTHRESH)
    {
      error += ", RTD High Threshold";
    }
    if (fault & MAX31865_FAULT_LOWTHRESH)
    {
      error += ", RTD Low Threshold";
    }
    if (fault & MAX31865_FAULT_REFINLOW)
    {
      error += ", REFIN- > 0.85 x Bias";
    }
    if (fault & MAX31865_FAULT_REFINHIGH)
    {
      error += ", REFIN- < 0.85 x Bias - FORCE- open";
    }
    if (fault & MAX31865_FAULT_RTDINLOW)
    {
      error += ", RTDIN- < 0.85 x Bias - FORCE- open";
    }
    if (fault & MAX31865_FAULT_OVUV)
    {
      error += ", Under/Over voltage";
    }

    jsonBuilder.addKeyValue(F("MAX31865"), (int)fault);
    Serial.println(error);
    return T_UNDEFINED;
  }

  return round(temperature); // 1 degree precision is enough
}