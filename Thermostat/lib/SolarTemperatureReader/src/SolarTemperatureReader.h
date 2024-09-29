#ifndef _SOLAR_TEMPERATURE_READER_H
#define _SOLAR_TEMPERATURE_READER_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>
#include <Adafruit_MAX31865.h> // https://github.com/adafruit/Adafruit_MAX31865

class SolarTemperatureReader
{
public:
  SolarTemperatureReader(int8_t spi_cs);

  float getSolarPaneTemperature(uint8_t &fault);

private:
  Adafruit_MAX31865* _solarSensor;
  uint8_t _errorCount;
  float _prevValue;
};

#endif