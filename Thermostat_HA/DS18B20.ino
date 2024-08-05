const byte MAX_DS1820_SENSORS = 3;

typedef struct
{
  DeviceAddress address;
  byte oneWireId;
} __attribute__((__packed__)) DS18B20Sensor;

typedef DS18B20Sensor DS18B20Sensors[MAX_DS1820_SENSORS];

// DS18B20 Temp. Sensor roles

const int TEMP_SENSOR_BOILER_BOTTOM = 0;
const int TEMP_SENSOR_BOILER_TOP = 1;
const int TEMP_SENSOR_FURNACE = 2;
//const int TEMP_SENSOR_RESERVE_1 = 3;

OneWire wire1(PIN_ONE_WIRE_BUS);
DallasTemperature dallasSensors(&wire1);

int ds18b20SensorCount = 0;

DS18B20Sensors tempSensors;


static int readTankBottomT() {
  return getTemperatureById(TEMP_SENSOR_BOILER_BOTTOM);
}

static int readTankTopT() {
  return getTemperatureById(TEMP_SENSOR_BOILER_TOP);
}

int readFurnaceT() {
  return getTemperatureById(TEMP_SENSOR_FURNACE);
}

bool getSensorAddress(DeviceAddress addr, int id) {
  return dallasSensors.getAddress(addr, id);
}

void readDS18B20SensorsData() {
  memset(&tempSensors, 0, sizeof(tempSensors));
  eeprom_read_block(&tempSensors, (uint8_t*)STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS, sizeof(tempSensors));
}

void saveDS18B20SensorsData() {
  eeprom_write_block(&tempSensors, (uint8_t*)STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS, sizeof(tempSensors));
  Serial.println(F("New DS18B20 settings applied"));
}

static bool isSameSensorAddress(const DeviceAddress addr1, const DeviceAddress addr2) {
  for (byte i = 0; i < sizeof(DeviceAddress); i++) {
    if (addr1[i] != addr2[i])
      return false;
  }
  return true;
}


static void copySensorAddress(const DeviceAddress addrFrom, DeviceAddress addrTo) {
  for (byte i = 0; i < sizeof(DeviceAddress); i++)
    addrTo[i] = addrFrom[i];
}

static void initDS18b20TempSensors() {
  readDS18B20SensorsData();

  for (byte j = 0; j < MAX_DS1820_SENSORS; j++)
    tempSensors[j].oneWireId = 0xFF;

  dallasSensors.begin();

  ds18b20SensorCount = dallasSensors.getDeviceCount();

  Serial.print(F("DS18B20 sensors found: "));
  Serial.println(ds18b20SensorCount);

  if (ds18b20SensorCount == 0)
    return;

  Serial.print(F("DS18B20 parasite mode: "));
  Serial.println(dallasSensors.isParasitePowerMode());

  dallasSensors.setWaitForConversion(false);


  DeviceAddress addr;
  // first pass
  for (byte i = 0; i < ds18b20SensorCount; i++) {
    if (dallasSensors.getAddress(addr, i)) {
      for (int j = 0; j < MAX_DS1820_SENSORS; j++) {
        if (isSameSensorAddress(tempSensors[j].address, addr)) {
          tempSensors[j].oneWireId = i;
          break;
        }
      }
    }
  }

  // second pass. fill gaps if possible
  for (byte i = 0; i < ds18b20SensorCount; i++) {
    if (dallasSensors.getAddress(addr, i)) {
      bool isUsed = false;
      for (int j = 0; j < MAX_DS1820_SENSORS; j++) {
        if (tempSensors[j].oneWireId == i) {
          isUsed = true;
          break;
        }
      }

      if (!isUsed) {
        for (int j = 0; j < MAX_DS1820_SENSORS; j++) {
          if (tempSensors[j].oneWireId == 0xFF) {
            tempSensors[j].oneWireId = i;
            copySensorAddress(addr, tempSensors[j].address);
            saveDS18B20SensorsData();
            break;
          }
        }
      }
    }
  }
}

void startDS18B20TemperatureMeasurements() {
  dallasSensors.requestTemperatures();
}

// Returns temperature multiplied by 10, or T_UNDEFINED
static int getTemperatureById(int id) {
  int T = T_UNDEFINED;
  if (tempSensors[id].oneWireId < 0xFF) {
    float Tx = dallasSensors.getTempC(tempSensors[id].address);
    if (Tx > DEVICE_DISCONNECTED_C)
      T = round(Tx) * 10;  // 1 degree precicion is enough
  }
  return T;
}
