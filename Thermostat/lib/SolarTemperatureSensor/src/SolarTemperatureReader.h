#ifndef _SOLAR_TEMPERATURE_READER_H
#define _SOLAR_TEMPERATURE_READER_H

#include <ArduinoHA.h>
#include <Adafruit_MAX31865.h> // https://github.com/adafruit/Adafruit_MAX31865
#include <ArduinoJson.h>

class SolarTemperatureReader
{
public:
  SolarTemperatureReader(int8_t spi_cs);

  float getSolarPaneTemperature(JsonDocument &jsonDoc);

private:
  Adafruit_MAX31865* _solarSensor;
};

#endif