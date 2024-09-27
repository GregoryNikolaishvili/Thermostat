#include "TemperatureDS18B20.h"
#include <OneWire.h>

TemperatureDS18B20::TemperatureDS18B20(uint8_t oneWireBusPin, int dataStorageAddress)
{
  _oneWireBusPin = oneWireBusPin;
  _dataStorageAddress = dataStorageAddress;
  _ds18b20SensorCount = 0;
}

void TemperatureDS18B20::initialize()
{
  pinMode(_oneWireBusPin, INPUT_PULLUP);

  readDS18B20SensorsData();
  for (byte j = 0; j < MAX_DS1820_SENSORS; j++)
    _sensorDataArray[j].oneWireId = 0xFF;

  OneWire *wire1 = new OneWire(_oneWireBusPin);
  _dallasSensors = new DallasTemperature(wire1);

  _dallasSensors->begin();
  _ds18b20SensorCount = _dallasSensors->getDeviceCount();

  Serial.print(F("DS18B20 sensors found: "));
  Serial.println(_ds18b20SensorCount);

  if (_ds18b20SensorCount == 0)
    return;

  Serial.print(F("DS18B20 parasite mode: "));
  Serial.println(_dallasSensors->isParasitePowerMode());

  _dallasSensors->setWaitForConversion(false);

  startMeasurements();

  DeviceAddress addr;

  // first pass
  for (byte i = 0; i < _ds18b20SensorCount; i++)
  {
    if (_dallasSensors->getAddress(addr, i))
    {
      for (int j = 0; j < MAX_DS1820_SENSORS; j++)
      {
        if (isSameSensorAddress(_sensorDataArray[j].address, addr))
        {
          _sensorDataArray[j].oneWireId = i;
          break;
        }
      }
    }
  }

  // second pass. fill gaps if possible
  for (byte i = 0; i < _ds18b20SensorCount; i++)
  {
    if (_dallasSensors->getAddress(addr, i))
    {
      bool isUsed = false;
      for (int j = 0; j < MAX_DS1820_SENSORS; j++)
      {
        if (_sensorDataArray[j].oneWireId == i)
        {
          isUsed = true;
          break;
        }
      }

      if (!isUsed)
      {
        for (int j = 0; j < MAX_DS1820_SENSORS; j++)
        {
          if (_sensorDataArray[j].oneWireId == 0xFF)
          {
            _sensorDataArray[j].oneWireId = i;
            copySensorAddress(addr, _sensorDataArray[j].address);
            saveDS18B20SensorsData();
            break;
          }
        }
      }
    }
  }
}

void TemperatureDS18B20::startMeasurements()
{
  _dallasSensors->requestTemperatures();
}

// Returns temperature multiplied or T_UNDEFINED
float TemperatureDS18B20::getTemperature(byte id)
{
  if (_ds18b20SensorCount > 0)
  {
    if (_sensorDataArray[id].oneWireId < 0xFF)
    {
      float Tx = _dallasSensors->getTempC(_sensorDataArray[id].address);
      if (Tx > DEVICE_DISCONNECTED_C)
        return round(Tx); // 1 degree precicion is enough
    }
    else
    {
      initialize();
    }
  }
  return NAN;
}

void TemperatureDS18B20::readDS18B20SensorsData()
{
#if defined(__AVR_ATmega2560__)
  memset(&_sensorDataArray, 0, sizeof(_sensorDataArray));
  eeprom_read_block(&_sensorDataArray, (uint8_t *)_dataStorageAddress, sizeof(_sensorDataArray));
#endif
}

bool TemperatureDS18B20::isSameSensorAddress(const DeviceAddress addr1, const DeviceAddress addr2)
{
  for (byte i = 0; i < sizeof(DeviceAddress); i++)
  {
    if (addr1[i] != addr2[i])
      return false;
  }
  return true;
}

void TemperatureDS18B20::copySensorAddress(const DeviceAddress addrFrom, DeviceAddress addrTo)
{
  for (byte i = 0; i < sizeof(DeviceAddress); i++)
    addrTo[i] = addrFrom[i];
}

void TemperatureDS18B20::saveDS18B20SensorsData()
{
#if defined(__AVR_ATmega2560__)
  eeprom_write_block(&_sensorDataArray, (uint8_t *)_dataStorageAddress, sizeof(_sensorDataArray));
  Serial.println(F("New DS18B20 settings applied"));
#endif
}

// bool getSensorAddress(DeviceAddress addr, int id) {
//   return _dallasSensors->getAddress(addr, id);
// }
