#ifndef _TEMPERATURE_DS18B20_H
#define _TEMPERATURE_DS18B20_H

#include <ProjectDefines.h>
#include <Arduino.h>
#include <DallasTemperature.h>

typedef struct
{
  DeviceAddress address;
  byte oneWireId;
} __attribute__((__packed__)) DS18B20SensorData;

// #ifndef T_UNDEFINED
//   #define T_UNDEFINED 999
// #endif

#define MAX_DS1820_SENSORS 4

typedef DS18B20SensorData DS18B20SensorDataArray [MAX_DS1820_SENSORS];

class TemperatureDS18B20
{
public:
    TemperatureDS18B20(uint8_t oneWireBusPin, int dataStorageAddress);

    void initialize();
    void startMeasurements();
    float getTemperature(byte id);
private:
    uint8_t _oneWireBusPin;
    int _dataStorageAddress;

    int _ds18b20SensorCount;
    DallasTemperature* _dallasSensors;

    DS18B20SensorDataArray _sensorDataArray;

    void readDS18B20SensorsData();
    bool isSameSensorAddress(const DeviceAddress addr1, const DeviceAddress addr2);
    void copySensorAddress(const DeviceAddress addrFrom, DeviceAddress addrTo);
    void saveDS18B20SensorsData();
};

#endif