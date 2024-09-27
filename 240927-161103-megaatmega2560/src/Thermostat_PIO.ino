//ქვედა რიგი მარცხნიდან მარჯვნივ(სულ 8)
//
//1. სამზარეულო
//2. ზალა 1 სამზარეულოსკენ როა
//3. ზალა 1 ეზოსკენ რომ იყურება
//4. კიბე ქვედა
//5. ქვედა ტუალეტი
//6. ზალა 1 კუთხეში
//7.  ბარის შესასვლელი
//8. ბარის ტუალეტი
//
//ზედა რიგი მარცხნიდან მარჯვნივ(სულ 12)
//
//1. ზედა კიბეები
//2. ზედა ტუალეტის საშრობი
//3. ზედა ტუალეტის რადიატორი
//4. ზალა 2 დიდი
//5. ზალა 2 პატარა
//6. გიო 1 და 2
//7. ნანა
//8. გიო 3
//9. გია
//10. ბარი(ბასეინისკენ)
//11. კინოთეატრი ქვედა ეზოსკენ
//12.  კინოთეატრი უკნისკენ(ფიჭვი)

#define REQUIRESALARMS false // FOR DS18B20 library

#include "Thermostat.h"

#include <TimeLib.h>				// https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>				// https://github.com/PaulStoffregen/TimeAlarms
#include <DS1307RTC.h>				// https://github.com/PaulStoffregen/DS1307RTC

#include <SPI.h>
#include <Ethernet.h>				// https://github.com/arduino-libraries/Ethernet
#include <PubSubClient.h>			// https://github.com/knolleary/pubsubclient

#include <Adafruit_MAX31865.h>		// https://github.com/adafruit/Adafruit_MAX31865
#include <OneWire.h>				// https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>	    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <MovingAverageFilter.h>
#include <Temperature.h>

#include <avr/wdt.h>

extern Adafruit_MAX31865 solarSensor;

byte heaterRelayPins[HEATER_RELAY_COUNT] = {
	PIN_HR_1,
	PIN_HR_2,
	PIN_HR_3,
	PIN_HR_4,

	PIN_HR_5,
	PIN_HR_6,
	PIN_HR_7,
	PIN_HR_8,

	PIN_HR_9,
	PIN_HR_10,
	PIN_HR_11,
	PIN_HR_12,

	PIN_HR_13,
	PIN_HR_14,
	PIN_HR_15,
	PIN_HR_16
};

byte heaterRelayState[HEATER_RELAY_COUNT];

bool poolPumpIsOn;

byte boilerRelayPins[BOILER_RELAY_COUNT] = {
	PIN_BL_SOLAR_PUMP,
	PIN_BL_CIRC_PUMP,
	PIN_BL_OVEN_PUMP,
	PIN_BL_OVEN_FAN
};

unsigned long halfSecondTicks = 0;
unsigned long secondTicks = 0;

//unsigned int thermostatControllerState = 0;

//uint16_t lastReadSolarPanelRTD;

BoilerSettingStructure boilerSettings;

int setHexT(char *buffer, int T, int idx)
{
	T = T + 1000;
	idx = setHexByte(buffer, T >> 8, idx);
	idx = setHexByte(buffer, T & 0xFF, idx);
	return idx;
}

int readHexT(const char* s)
{
	int value = readHex(s, 4);
	return value - 1000;
}

long readHexInt32(const char* s)
{
	long value = 0;
	int length = 8;
	while (length > 0)
	{
		value = (value << 4) | hexCharToByte(*s++);
		length--;
	}
	return value;
}

int setHexByte(char *buffer, byte x, int idx)
{
  buffer[idx++] = byteToHexChar((x >> 4) & 0x0F);
  buffer[idx++] = byteToHexChar(x & 0x0F);
  return idx;
}

int setHexInt16(char *buffer, int x, int idx)
{
  idx = setHexByte(buffer, x >> 8, idx);
  idx = setHexByte(buffer, x & 0xFF, idx);
  return idx;
}

int setHexInt32(char *buffer, long x, int idx)
{
	idx = setHexInt16(buffer, x >> 16, idx);
	idx = setHexInt16(buffer, x & 0xFFFF, idx);
	return idx;
}

//void printHexByte(Print* client, byte x)
//{
//  client->print(byte((x >> 4) & 0x0F), HEX);
//  client->print(byte(x & 0x0F), HEX);
//}

//void printHexInt16(Print* client, int x)
//{
//  printHexByte(client, byte((x >> 8)));
//  printHexByte(client, byte(x & 0xFF));
//}

//void print2Digits(Print* client, byte x)
//{
//  if (x < 10)
//    client->print('0');
//  client->print(x);
//}

//static const char hex[] PROGMEM = "0123456789abcdef";
byte hexCharToByte(char c)
{
  if (c > '9')
    c -= ('A' - '9' - 1);
  return c - '0';
}

char byteToHexChar(byte b)
{
  if (b > 9)
    return b + 'A' - 10;
  return b + '0';
}

int readHex(const char* s, byte length)
{
  int value = 0;
  while (length > 0)
  {
    value = (value << 4) | hexCharToByte(*s++);
    length--;
  }

  return value;
}

void printDateTime(Print* client, time_t value)
{
  client->print(day(value));
  client->print(' ');
  client->print(monthShortStr(month(value)));
  client->print(' ');
  client->print(year(value));
  client->print(' ');

  if (hour(value) < 10)
    client->print('0');
  client->print(hour(value));
  client->print(':');
  if (minute(value) < 10)
    client->print('0');
  client->print(minute(value));
  client->print(':');
  if (second(value) < 10)
    client->print('0');
  client->print(second(value));
  client->println();
}

void printTime(Print* client, time_t value)
{
  if (hour(value) < 10)
    client->print('0');
  client->print(hour(value));
  client->print(':');
  if (minute(value) < 10)
    client->print('0');
  client->print(minute(value));
  client->print(':');
  if (second(value) < 10)
    client->print('0');
  client->print(second(value));
  client->print(' ');
}

const byte MAX_DS1820_SENSORS = 3;

typedef struct
{
	DeviceAddress address;
	byte oneWireId;
}  __attribute__((__packed__)) DS18B20Sensor;

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


int readTankBottomT()
{
	return getTemperatureById(TEMP_SENSOR_BOILER_BOTTOM);
}

int readTankTopT()
{
	return getTemperatureById(TEMP_SENSOR_BOILER_TOP);
}

int readFurnaceT()
{
	return getTemperatureById(TEMP_SENSOR_FURNACE);
}

bool getSensorAddress(DeviceAddress addr, int id)
{
	return dallasSensors.getAddress(addr, id);
}

void readDS18B20SensorsData()
{
	memset(&tempSensors, 0, sizeof(tempSensors));
	eeprom_read_block(&tempSensors, (uint8_t*)STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS, sizeof(tempSensors));
}

void saveDS18B20SensorsData()
{
	eeprom_write_block(&tempSensors, (uint8_t*)STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS, sizeof(tempSensors));
	Serial.println(F("New DS18B20 settings applied"));
}

bool isSameSensorAddress(const DeviceAddress addr1, const DeviceAddress addr2)
{
	for (byte i = 0; i < sizeof(DeviceAddress); i++)
	{
		if (addr1[i] != addr2[i])
			return false;
	}
	return true;
}


void copySensorAddress(const DeviceAddress addrFrom, DeviceAddress addrTo)
{
	for (byte i = 0; i < sizeof(DeviceAddress); i++)
		addrTo[i] = addrFrom[i];
}

void initDS18b20TempSensors()
{
	readDS18B20SensorsData();

	for (byte j = 0; j < MAX_DS1820_SENSORS; j++)
		tempSensors[j].oneWireId = 0xFF;

	dallasSensors.begin();

	ds18b20SensorCount = dallasSensors.getDeviceCount();

	Serial.print(F("DS18B20 sensors found: "));	Serial.println(ds18b20SensorCount);

	if (ds18b20SensorCount == 0)
		return;

	Serial.print(F("DS18B20 parasite mode: "));	Serial.println(dallasSensors.isParasitePowerMode());

	dallasSensors.setWaitForConversion(false);


	DeviceAddress addr;
	// first pass
	for (byte i = 0; i < ds18b20SensorCount; i++)
	{
		if (dallasSensors.getAddress(addr, i))
		{
			for (int j = 0; j < MAX_DS1820_SENSORS; j++)
			{
				if (isSameSensorAddress(tempSensors[j].address, addr))
				{
					tempSensors[j].oneWireId = i;
					break;
				}
			}
		}
	}

	// second pass. fill gaps if possible
	for (byte i = 0; i < ds18b20SensorCount; i++)
	{
		if (dallasSensors.getAddress(addr, i))
		{
			bool isUsed = false;
			for (int j = 0; j < MAX_DS1820_SENSORS; j++)
			{
				if (tempSensors[j].oneWireId == i)
				{
					isUsed = true;
					break;
				}
			}

			if (!isUsed)
			{
				for (int j = 0; j < MAX_DS1820_SENSORS; j++)
				{
					if (tempSensors[j].oneWireId == 0xFF)
					{
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

void startDS18B20TemperatureMeasurements()
{
	dallasSensors.requestTemperatures();
}

// Returns temperature multiplied by 10, or T_UNDEFINED
int getTemperatureById(int id)
{
	int T = T_UNDEFINED;
	if (tempSensors[id].oneWireId < 0xFF)
	{
		float Tx = dallasSensors.getTempC(tempSensors[id].address);
		if (Tx > DEVICE_DISCONNECTED_C)
			T = round(Tx) * 10; // 1 degree precicion is enough
	}
	return T;
}

//int getTemperatureByOneWireId(int id)
//{
//	int T = T_UNDEFINED;
//	float Tx = dallasSensors.getTempCByIndex(id);
//	if (Tx > DEVICE_DISCONNECTED_C)
//		T = round(Tx) * 10;  // 1 degree precicion is enough
//	return T;
//}


Adafruit_MAX31865 solarSensor = Adafruit_MAX31865(PIN_MAX31865_SELECT);

Temperature* boilerSensorsValues[BOILER_SENSOR_COUNT];

bool isBoilerTankOverheated = false;

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;

bool warning_SolarSensorFail_IsActivated = false;
bool warning_TankBottomSensorFail_IsActivated = false;
bool warning_TankTopSensorFail_IsActivated = false;
bool warning_HelioPressure_IsActivated = false;

void InitTemperatureSensors()
{
	solarSensor.begin(MAX31865_2WIRE);
	solarSensor.enable50Hz(true);

	for (int i = 0; i < BOILER_SENSOR_COUNT; i++)
		boilerSensorsValues[i] = new Temperature(6);

	initDS18b20TempSensors();

	solarSensor.readRTD();
	startDS18B20TemperatureMeasurements();
}

void ReInitBoiler()
{
	if (boilerSettings.Mode != BOILER_MODE_WINTER)
	{
		for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
			heaterRelaySetValue(id, 100); // 100% open
		circPumpPumpOff();
		burnerOff();
		return;
	}
}

int getBoilerT(byte id)
{
	return boilerSensorsValues[id]->getCurrentValue();
}

int setBoilerT(byte id, int value)
{
	Temperature* temperature = boilerSensorsValues[id];
	int old = temperature->getCurrentValue();
	int newValue = temperature->process(value);
	if (old != newValue)
		PublishBoilerSensorT(id);
	return newValue;
}

//void CheckCirculatingPump()
//{
//	if (checkAllHeaterRelaysAreOff())
//		circPumpPumpOff();
//	else
//		circPumpPumpOn();
//}

// boiler & solar
void ProcessTemperatureSensors(bool& publishError)
{
	int TSolar = readSolarPaneT(); // Solar panel T

	int TBottom = readTankBottomT(); // Tank bottom
	int TTop = readTankTopT(); // Tank top

	int TF = readFurnaceT();

	//	Serial.print("T1 = "); Serial.println(TSolar);
	//	Serial.print("T2 = "); Serial.println(T2);
	//	Serial.print("T3 = "); Serial.println(T3);
	//	Serial.print("T_furnace = "); Serial.println(TF);

	if (!isValidT(TSolar))
	{
		if (!warning_SolarSensorFail_IsActivated)
		{
			publishError = true;
			warning_SolarSensorFail_IsActivated = true;
		}
	}
	else
	{
		if (warning_SolarSensorFail_IsActivated)
		{
			publishError = true;
			warning_SolarSensorFail_IsActivated = false;
		}
	}

	if (!isValidT(TBottom))
	{
		if (!warning_TankBottomSensorFail_IsActivated)
		{
			publishError = true;
			warning_TankBottomSensorFail_IsActivated = true;
		}
	}
	else
	{
		if (warning_TankBottomSensorFail_IsActivated)
		{
			publishError = true;
			warning_TankBottomSensorFail_IsActivated = false;
		}
	}

	if (!isValidT(TTop))
	{
		if (!warning_TankTopSensorFail_IsActivated)
		{
			publishError = true;
			warning_TankTopSensorFail_IsActivated = true;
		}
	}
	else
	{
		if (warning_TankTopSensorFail_IsActivated)
		{
			publishError = true;
			warning_TankTopSensorFail_IsActivated = false;
		}
	}

	TSolar = setBoilerT(T_SOLAR_PANEL_T, TSolar);
	TBottom = setBoilerT(T_TANK_BOTTOM, TBottom);
	TTop = setBoilerT(T_TANK_TOP, TTop);
	TF = setBoilerT(T_FURNACE, TF);

	// Read sensor data
	if (!isValidT(TSolar))
	{
		solarPumpOn();
	}

	if (!isValidT(TTop))
	{
		TTop = TBottom;
	}

	if (!isValidT(TBottom) && isValidT(TTop))
	{
		TBottom = TTop;
	}

	if (!isValidT(TF))
	{
		burnerOff();
	}

	int TBoiler = TTop;
	if (TBottom > TTop)
		TBoiler = TBottom;

	////////////////// End of Read sensor data

	bool isSolarOk = CheckSolarPanels(TSolar, publishError);

	CheckBoilerTank(TBoiler, publishError);

	if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > boilerSettings.CollectorMinimumSwitchOnT) // T2 = Tank bottom
	{
		if (!isSolarPumpOn() && (TSolar >= TBottom + boilerSettings.CollectorSwitchOnTempDiff))
		{
			solarPumpOn();
		}
		else
		{
			if (isSolarPumpOn() && (TSolar < TBottom + boilerSettings.CollectorSwitchOffTempDiff))
			{
				solarPumpOff();
			}
		}
	}

	if (!isBoilerTankOverheated && isValidT(TTop)) // T3 = Tank top
	{
		if (boilerSettings.Mode == BOILER_MODE_SUMMER_POOL)
		{
			if (!poolPumpIsOn)
			{
				circPumpPumpOff();
			}
			else
			{
				if (TTop >= boilerSettings.PoolSwitchOnT)
					circPumpPumpOn();
				else if (TTop < boilerSettings.PoolSwitchOffT)
					circPumpPumpOff();
			}
		}
	}
}

bool CheckSolarPanels(int TSolar, bool& publishError)
{
	if (isValidT(TSolar))
	{
		// Is solar collector too hot?
		if (TSolar >= boilerSettings.CollectorEmergencySwitchOffT || // 140
			(warning_EMOF_IsActivated && TSolar >= boilerSettings.CollectorEmergencySwitchOnT)) // 120
		{
			solarPumpOff();
			if (!warning_EMOF_IsActivated)
			{
				warning_EMOF_IsActivated = true;
				publishError = true;
				Serial.println(F("EMOF activated"));
			}
			return false;
		}

		if (warning_EMOF_IsActivated)
		{
			warning_EMOF_IsActivated = false;
			publishError = true;
			Serial.println(F("EMOF deactivated"));
		}

		// Is solar collector too cold?
		if (TSolar <= boilerSettings.CollectorAntifreezeT) // 4
		{
			solarPumpOn();
			if (!warning_CFR_IsActivated)
			{
				warning_CFR_IsActivated = true;
				publishError = true;
				Serial.println(F("CFR activated"));
			}
			return false;
		}

		if (warning_CFR_IsActivated)
		{
			warning_CFR_IsActivated = false;
			publishError = true;
			Serial.println(F("CFR deactivated"));
		}
	}

	return true;
}

void CheckBoilerTank(int TBoiler, bool& publishError)
{
	if (!isValidT(TBoiler))
	{
		burnerOff();
		return;
	}

	bool turnBurnerOn = true;
	if (TBoiler >= boilerSettings.MaxTankT ||
		(warning_SMX_IsActivated && TBoiler >= boilerSettings.MaxTankT - 50)) // SMX, 60
	{
		turnBurnerOn = false;
		burnerOff();

		if (!warning_SMX_IsActivated)
		{
			warning_SMX_IsActivated = true;
			publishError = true;
			Serial.println(F("SMX activated"));
		}
	}
	else
	{
		if (warning_SMX_IsActivated)
		{
			warning_SMX_IsActivated = false;
			publishError = true;
			Serial.println(F("SMX deactivated"));
		}
	}

	if (TBoiler >= boilerSettings.AbsoluteMaxTankT || (warning_MAXT_IsActivated && TBoiler >= boilerSettings.AbsoluteMaxTankT - 50)) // 95 degree is absolute max for boiler tank
	{
		isBoilerTankOverheated = true;
		turnBurnerOn = false;
		burnerOff();

		// Try to cool down boiler with all means
		for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
			heaterRelaySetValue(id, 100); // 100% open

		circPumpPumpOn(); // Turn on recirculating pump

		if (!warning_MAXT_IsActivated)
		{
			warning_MAXT_IsActivated = true;
			publishError = true;
			Serial.println(F("95 degree in tank activated"));
		}
		return;
	}

	isBoilerTankOverheated = false;
	if (warning_MAXT_IsActivated)
	{
		warning_MAXT_IsActivated = false;
		publishError = true;
		Serial.println(F("95 degree in tank deactivated"));
	}
	if (turnBurnerOn)
		burnerOn();
}

// Returns temperature multiplied by 10, or T_UNDEFINED
int readSolarPaneT()
{
	//const float RREF = 4300.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
	//const float RREF = 4265.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
	const float RREF = 4000.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code

	float Rt = solarSensor.readRTD();
	Rt /= 32768;
	Rt *= RREF;

	//Serial.println(Rt);

	float temperature = solarSensor.temperature(1000.0, RREF);

	//Serial.println(temperature);

	//Serial.print("RTD value: "); Serial.println(lastReadSolarPanelRTD);
	//Serial.print(F("RTD Resistance = ")); Serial.println(RREF * lastReadSolarPanelRTD / 32768, 8);
	//Serial.print(F("RTD Temperature = ")); Serial.println(temperature);

	// Check and print any faults
	uint8_t fault = solarSensor.readFault();
	if (fault) {
		solarSensor.clearFault();

		Serial.print(F("Fault 0x")); Serial.println(fault, HEX);
		if (fault & MAX31865_FAULT_HIGHTHRESH) {
			Serial.println(F("RTD High Threshold"));
		}
		if (fault & MAX31865_FAULT_LOWTHRESH) {
			Serial.println(F("RTD Low Threshold"));
		}
		if (fault & MAX31865_FAULT_REFINLOW) {
			Serial.println(F("REFIN- > 0.85 x Bias"));
		}
		if (fault & MAX31865_FAULT_REFINHIGH) {
			Serial.println(F("REFIN- < 0.85 x Bias - FORCE- open"));
		}
		if (fault & MAX31865_FAULT_RTDINLOW) {
			Serial.println(F("RTDIN- < 0.85 x Bias - FORCE- open"));
		}
		if (fault & MAX31865_FAULT_OVUV) {
			Serial.println(F("Under/Over voltage"));
		}

		return T_UNDEFINED;
	}

	return round(temperature) * 10; // 1 degree precision is enough
}

//bool checkAllHeaterRelaysAreOff()
//{
//	bool allHeaterRelaysAreOff = true;
//
//	for (byte i = 0; i < USED_HEATER_RELAY_COUNT; i++)
//	{
//		if (heaterRelayGetValue(i) > 0)
//		{
//			allHeaterRelaysAreOff = false;
//			break;
//		}
//	}
//
//	return allHeaterRelaysAreOff;
//}

void setup()
{
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Initializing.. ver. 3.1.2"));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW); // Turn on led at start

	pinMode(PIN_MANUAL_MODE_LED, OUTPUT);
	digitalWrite(PIN_MANUAL_MODE_LED, LOW); // Turn on manual mode led at start

	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH); // Disable SD card

	pinMode(PIN_ETHERNET_SS, OUTPUT);
	digitalWrite(PIN_ETHERNET_SS, HIGH); // Disable Ethernet

	pinMode(PIN_MAX31865_SELECT, OUTPUT);
	digitalWrite(PIN_MAX31865_SELECT, HIGH); // Disable MAX31865

	// Init relays
	for (byte i = 0; i < HEATER_RELAY_COUNT; i++)
	{
		heaterRelayState[i] = 100; // Open all thermo valves to 100% (NO)
		digitalWrite(heaterRelayPins[i], LOW);
		pinMode(heaterRelayPins[i], OUTPUT);
	}

	for (byte i = 0; i < BOILER_RELAY_COUNT; i++)
	{
		digitalWrite(boilerRelayPins[i], HIGH);
		pinMode(boilerRelayPins[i], OUTPUT);
	}

	pinMode(PIN_PRESSURE_SENSOR, INPUT);

	if (dtNBR_ALARMS != 30)
		Serial.println(F("Alarm count mismatch"));

	setTime(0, 0, 1, 1, 1, 2001);

	InitTemperatureSensors();
	InitHelioPressureSensor();

	readSettings();

	delay(1000);
	InitEthernet();

	wdt_enable(WDTO_8S);

	bool publishError = false;
	ProcessTemperatureSensors(publishError);
	ProcessHelioPressure(publishError);

	wdt_disable();

	delay(2000);

	InitMqtt();

	wdt_enable(WDTO_8S);

	Serial.println(F("Start"));
}

void loop()
{
	static unsigned long previousMillis = 0; // will store last time LED was updated
	unsigned long _current_millis = millis();

	uint32_t dt = previousMillis > _current_millis ? 1 + previousMillis + ~_current_millis : _current_millis - previousMillis;

	if (dt >= 500)
	{
		wdt_reset();

		// save the last time we blinked the LED
		previousMillis = _current_millis;
		oncePerHalfSecond();
	}

	ProcessMqtt();

	Alarm.delay(0);
}

void oncePerHalfSecond(void)
{
	halfSecondTicks++;

	// Blinking
	static uint8_t blinkingLedState = LOW;

	blinkingLedState = ~blinkingLedState;
	digitalWrite(PIN_BLINKING_LED, blinkingLedState);

	if ((halfSecondTicks + 4) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) // 2 second before processing temperatures
		startDS18B20TemperatureMeasurements();

	if (halfSecondTicks % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0)
	{
		bool publishError = false;

		ProcessTemperatureSensors(publishError);
		ProcessHelioPressure(publishError);

		if (publishError)
			PublishBoilerSettings();
	}

	if ((halfSecondTicks % 2) == 0)
		oncePerSecond();
}

void oncePerSecond()
{
	processHeaterRelays();

	if ((secondTicks % 5) == 0)
		oncePer5Second();

	if ((secondTicks % 60) == 0)
		oncePer1Minute();

	secondTicks++;
}

void oncePer5Second()
{
	//	static byte heaterId = 0;
	//  for (byte i = 0; i < HEATER_RELAY_COUNT; i++)
	//  {
	//    digitalWrite(heaterRelayPins[i], LOW);
	//  }
	//
	//   digitalWrite(heaterRelayPins[0], HIGH);
	//
	//  if (heaterId == 0)
	//    Serial.println(" ------------------------------ ");
	//    Serial.println(heaterId + 1);
	//
	//  heaterId++;
	//  if (heaterId == HEATER_RELAY_COUNT)
	//    heaterId = 0;
		//ReconnectMqtt();
}

void oncePer1Minute()
{
	ReconnectMqtt();

	if (secondTicks > 0) // do not publish on startup
		PublishAllStates();
}

void heaterRelaySetValue(byte id, byte value)
{
	if (id < HEATER_RELAY_COUNT)
	{
		if (heaterRelayState[id] != value)
		{
			heaterRelayState[id] = value;

			PublishHeaterRelayState(id, value);
		}
	}
}

byte heaterRelayGetValue(byte id)
{
	if (id < HEATER_RELAY_COUNT)
	{
		return heaterRelayState[id];
	}
	return 0;
}

void processHeaterRelays()
{
	for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
	{
		if (heaterRelayState[id])
		{
			digitalWrite(heaterRelayPins[id], LOW);  // OPEN
		}
		else
		{
			digitalWrite(heaterRelayPins[id], HIGH); // CLOSE
		}
	}

	//static unsigned long windowStartTime = 0;

	//if (secondTicks - windowStartTime >= 100) // time to shift the Relay Window
	//	windowStartTime = secondTicks;

	//Serial.println(secondTicks - windowStartTime);

	//for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
	//{
	//	Serial.print("Heater relay #");
	//	Serial.print(id + 1);
	//	Serial.print(", State=");
	//	Serial.println(heaterRelayState[id]);

	//	if (heaterRelayState[id] > (int)(secondTicks - windowStartTime))
	//	{
	//		digitalWrite(heaterRelayPins[id], LOW);  // OPEN
	//	}
	//	else
	//	{
	//		digitalWrite(heaterRelayPins[id], HIGH); // CLOSE
	//	}
	//}
}

void solarPumpOn()
{
	_boilerRelayOn(BL_SOLAR_PUMP);
}

void solarPumpOff()
{
	_boilerRelayOff(BL_SOLAR_PUMP);
}

boolean isSolarPumpOn()
{
	return _isBoilerRelayOn(BL_SOLAR_PUMP);
}

AlarmID_t circPumpStartingAlarm = 0xFF;

boolean circPumpStarting()
{
	return circPumpStartingAlarm < 0xFF;
}

void circPumpPumpOn()
{
	if (circPumpStarting() || _isBoilerRelayOn(BL_CIRC_PUMP))
		return;

	if (boilerSettings.Mode == BOILER_MODE_WINTER) // needs 5 min delay
	{
		circPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, circPumpOnTimer, "", 0);

		PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
	}
	else
	{
		_boilerRelayOn(BL_CIRC_PUMP);
	}
}

void circPumpOnTimer(int tag, int tag2)
{
	if (circPumpStartingAlarm < 0xFF)
	{
		Alarm.free(circPumpStartingAlarm);
		circPumpStartingAlarm = 0xFF;
	}
	_boilerRelayOn(BL_CIRC_PUMP);
}

void circPumpPumpOff()
{
	if (circPumpStartingAlarm < 0xFF)
	{
		Alarm.free(circPumpStartingAlarm);
		circPumpStartingAlarm = 0xFF;
		PublishBoilerRelayState(BL_CIRC_PUMP, false);
	}
	else
	{
		_boilerRelayOff(BL_CIRC_PUMP);
	}
}

void burnerOn() //TODO: never used
{
	_boilerRelayOn(BL_FURNACE);
}

void burnerOff()
{
	_boilerRelayOff(BL_FURNACE);
}

void _boilerRelaySet(byte id, bool state)
{
	if (state)
		_boilerRelayOn(id);
	else
		_boilerRelayOff(id);
}

void _boilerRelayOn(byte id)
{
	if (_isBoilerRelayOn(id))
		return;

	if (id < BOILER_RELAY_COUNT)
	{
		digitalWrite(boilerRelayPins[id], LOW);
		PublishBoilerRelayState(id, true);
	}
}

void _boilerRelayOff(byte id)
{
	if (!_isBoilerRelayOn(id))
		return;

	if (id < BOILER_RELAY_COUNT)
	{
		digitalWrite(boilerRelayPins[id], HIGH);
		PublishBoilerRelayState(id, false);
	}
}

bool _isBoilerRelayOn(byte id)
{
	if (id < BOILER_RELAY_COUNT)
		return !digitalRead(boilerRelayPins[id]);
	return false;
}

void processPoolPumpState(byte id, bool value)
{
	if (id != 0)
		return;

	poolPumpIsOn = value;

	bool publishError = false;
	ProcessTemperatureSensors(publishError);
}

const char* MqttUserName = "cha";
const char* MqttPassword = "BatoBato02@";

IPAddress ip(192, 168, 68, 8);
IPAddress gateway(192, 168, 68, 1);
IPAddress subnet(255, 255, 252, 0);

EthernetClient ethClient;
PubSubClient mqttClient("192.168.68.23", 1883, callback, ethClient);     // Initialize a MQTT mqttClient instance

byte mac[] = { 0x54, 0x34, 0x41, 0x30, 0x30, 0x08 };

#define MQTT_BUFFER_SIZE 256
char buffer[MQTT_BUFFER_SIZE];

bool doLog = true;

void InitEthernet()
{
	Serial.println(F("Starting ethernet.."));

	Ethernet.begin(mac, ip, gateway, gateway, subnet);
	ethClient.setConnectionTimeout(2000);

	Ethernet.setRetransmissionTimeout(250);
	Ethernet.setRetransmissionCount(4);

	Serial.print(F("IP Address: "));
	Serial.println(Ethernet.localIP());
}

void InitMqtt()
{
	mqttClient.setBufferSize(320);
	mqttClient.setSocketTimeout(5);
	ReconnectMqtt();
}

void ProcessMqtt()
{
	mqttClient.loop();
}

void PublishMqtt(const char* topic, const char* message, int len, boolean retained)
{
	if (doLog)
	{
		Serial.print(F("Publish. topic="));
		Serial.print(topic);
		Serial.print(F(", length="));
		Serial.print(len);

		Serial.print(F(", payload="));
		for (int i = 0; i < len; i++)
			Serial.print(message[i]);
		Serial.println();
	}
	mqttClient.publish(topic, (byte*)message, len, retained);
}

void PublishAlive()
{
	if (!mqttClient.connected()) return;

	const char* topic = "cha/ts/alive";
	int len = setHexInt32(buffer, now() - 4L * SECS_PER_HOUR, 0);
	PublishMqtt(topic, buffer, len, false);
}

void ReconnectMqtt() {
	if (!mqttClient.connected()) {
		Serial.print(F("Connecting to MQTT broker..."));

		// Attempt to connect
		if (mqttClient.connect("boiler", MqttUserName, MqttPassword, "hub/controller/boiler", 1, true, "{\"state\":\"disconnected\"}")) {
			Serial.println(F("connected"));

			// Once connected, publish an announcement...
			mqttClient.publish("hub/controller/boiler", "{\"state\":\"connected\"}", true);  // Publish ethernet connected status to MQTT topic

			// ... and resubscribe
			mqttClient.subscribe("chac/ts/#", 1);     // Subscribe to a MQTT topic, qos = 1

			mqttClient.publish("hubcommand/gettime2", "chac/ts/settime2", false); // request time

			PublishBoilerSettings();
			PublishAllStates();
		}
		else {
			Serial.print(F("failed, rc="));
			Serial.println(mqttClient.state());
		}
	}
}

void PublishAllStates() {
	if (!mqttClient.connected()) return;

	doLog = false;

	for (byte id = 0; id < BOILER_SENSOR_COUNT; id++)
		PublishBoilerSensorT(id);

	for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
		PublishHeaterRelayState(id, heaterRelayGetValue(id));

	for (byte id = 0; id < BOILER_RELAY_COUNT; id++)
		PublishBoilerRelayState(id, _isBoilerRelayOn(id));

	PublishHelioPressure();

	doLog = true;
}

void PublishHeaterRelayState(byte id, byte value)
{
	if (!mqttClient.connected()) return;

	char topic[12];
	strcpy(topic, "cha/ts/hr/?");
	topic[10] = byteToHexChar(id);
	setHexByte(buffer, value, 0);
	PublishMqtt(topic, buffer, 2, true);
}

void PublishBoilerRelayState(byte id, bool value)
{
	if (!mqttClient.connected()) return;

	char topic[12];
	strcpy(topic, "cha/ts/br/?");
	topic[10] = byteToHexChar(id);
	bool b = !value && (id == BL_CIRC_PUMP) && circPumpStarting();
	PublishMqtt(topic, value ? "1" : (b ? "2" : "0"), 1, true);
}

void PublishBoilerSensorT(byte id)
{
	if (!mqttClient.connected()) return;

	char topic[12];
	strcpy(topic, "cha/ts/bs/?");
	topic[10] = byteToHexChar(id);

	Temperature* bsv = boilerSensorsValues[id];
	setHexT(buffer, bsv->getCurrentValue(), 0);

	PublishMqtt(topic, buffer, 4, true);
}

void PublishHelioPressure()
{
	if (!mqttClient.connected()) return;

	char topic[12];
	strcpy(topic, "cha/ts/hp/0");

	setHexInt16(buffer, getHelioPressure10(), 0);
	PublishMqtt(topic, buffer, 4, true);
}

//void PublishErrorCode(uint8_t fault)
//{
//	if (!mqttClient.connected()) return;
//
//	setHexInt16(buffer, fault, 0);
//	PublishMqtt("cha/ts/error", buffer, 4, true);
//}

void PublishTime()
{
	if (!mqttClient.connected()) return;

	const char* topic = "cha/ts/time";
	int len = setHexInt32(buffer, now() - 4L * SECS_PER_HOUR, 0);
	PublishMqtt(topic, buffer, len, false);
}

void PublishBoilerSettings()
{
	if (!mqttClient.connected()) return;

	const char* topic = "cha/ts/settings/bl";
	int idx = 0;

	buffer[idx++] = boilerSettings.Mode;
	idx = setHexT(buffer, boilerSettings.CollectorSwitchOnTempDiff, idx);
	idx = setHexT(buffer, boilerSettings.CollectorSwitchOffTempDiff, idx);
	idx = setHexT(buffer, boilerSettings.CollectorEmergencySwitchOffT, idx);
	idx = setHexT(buffer, boilerSettings.CollectorEmergencySwitchOnT, idx);
	idx = setHexT(buffer, boilerSettings.CollectorMinimumSwitchOnT, idx);
	idx = setHexT(buffer, boilerSettings.CollectorAntifreezeT, idx);
	idx = setHexT(buffer, boilerSettings.MaxTankT, idx);
	idx = setHexT(buffer, boilerSettings.AbsoluteMaxTankT, idx);

	idx = setHexT(buffer, boilerSettings.PoolSwitchOnT, idx);
	idx = setHexT(buffer, boilerSettings.PoolSwitchOffT, idx);

	idx = setHexInt16(buffer, warning_EMOF_IsActivated | 2 * warning_CFR_IsActivated | 4 * warning_SMX_IsActivated | 8 * warning_MAXT_IsActivated |
		16 * warning_SolarSensorFail_IsActivated | 32 * warning_TankBottomSensorFail_IsActivated | 64 * warning_TankTopSensorFail_IsActivated |
		128 * warning_HelioPressure_IsActivated, idx);

	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS1_Start, idx);
	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS1_End, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS1_SwitchOnT, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS1_SwitchOffT, idx);

	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS2_Start, idx);
	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS2_End, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS2_SwitchOnT, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS2_SwitchOffT, idx);

	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS3_Start, idx);
	//idx = setHexInt16(buffer, boilerSettings.BackupHeatingTS3_End, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS3_SwitchOnT, idx);
	//idx = setHexT(buffer, boilerSettings.BackupHeatingTS3_SwitchOffT, idx);

	PublishMqtt(topic, buffer, idx, true);
}

//void PublishAlert(char*message)
//{
//	PublishMqtt("cha/alert", message, strlen(message), false);
//	Serial.print("Alert: ");
//	if (message != NULL)
//		Serial.println(message);
//
//}

void callback(char* topic, byte* payload, unsigned int len) {
	Serial.print(F("message arrived: topic='"));
	Serial.print(topic);
	Serial.print(F("', length="));
	Serial.print(len);
	Serial.print(F(", payload="));
	Serial.write(payload, len);
	Serial.println();

	if (strcmp(topic, "chac/ts/alive") == 0)
	{
		PublishAlive();
		return;
	}

	if (strcmp(topic, "chac/ts/gettime2") == 0)
	{
		PublishTime();
		return;
	}

	if (strcmp(topic, "chac/ts/refresh") == 0)
	{
		PublishAllStates();
		return;
	}

	if (strcmp(topic, "chac/ts/diag") == 0)
	{
		publishTempSensorData();
		return;
	}

	if (len == 0)
		return;

	// Data arrived from pool relays via AcuLog
	if (strncmp(topic, "chac/ts/pl/", 11) == 0)
	{
		byte id = hexCharToByte(topic[11]);
		bool value = payload[0] != '0';

		processPoolPumpState(id, value);
		return;
	}

	if (strncmp(topic, "chac/ts/state/bl/", 17) == 0)
	{
		if (isBoilerTankOverheated)
			return;

		byte id = hexCharToByte(topic[17]);

		if (id == BL_CIRC_PUMP && payload[0] == '2' && !_isBoilerRelayOn(id) && !circPumpStarting()) // Start recirculation pump with delay
		{
			circPumpPumpOn();
			return;
		}

		bool value = payload[0] != '0';

		_boilerRelaySet(id, value);
		return;
	}

	if (strncmp(topic, "chac/ts/state/hr/", 17) == 0) // Heating
	{
		if (isBoilerTankOverheated)
			return;

		byte id = hexCharToByte(topic[17]);
		bool value = payload[0] != '0';

		heaterRelaySetValue(id, value ? 100 : 0);
		return;
	}

	if (strcmp(topic, "chac/ts/settings2/bl") == 0)
	{
		char* p = (char*)payload;

		Serial.println(F("New boiler settings"));

		boilerSettings.CollectorSwitchOnTempDiff = readHexT(p); p += 4;
		boilerSettings.CollectorSwitchOffTempDiff = readHexT(p); p += 4;
		boilerSettings.CollectorEmergencySwitchOffT = readHexT(p); p += 4;
		boilerSettings.CollectorEmergencySwitchOnT = readHexT(p); p += 4;
		boilerSettings.CollectorMinimumSwitchOnT = readHexT(p); p += 4;
		boilerSettings.CollectorAntifreezeT = readHexT(p); p += 4;
		boilerSettings.MaxTankT = readHexT(p); p += 4;
		boilerSettings.AbsoluteMaxTankT = readHexT(p); p += 4;

		boilerSettings.PoolSwitchOnT = readHexT(p); p += 4;
		boilerSettings.PoolSwitchOffT = readHexT(p); p += 4;

		//boilerSettings.BackupHeatingTS1_Start = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS1_End = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS1_SwitchOnT = readHexT(p); p += 4;
		//boilerSettings.BackupHeatingTS1_SwitchOffT = readHexT(p); p += 4;

		//boilerSettings.BackupHeatingTS2_Start = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS2_End = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS2_SwitchOnT = readHexT(p); p += 4;
		//boilerSettings.BackupHeatingTS2_SwitchOffT = readHexT(p); p += 4;

		//boilerSettings.BackupHeatingTS3_Start = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS3_End = readHex(p, 4); p += 4;
		//boilerSettings.BackupHeatingTS3_SwitchOnT = readHexT(p); p += 4;
		//boilerSettings.BackupHeatingTS3_SwitchOffT = readHexT(p); p += 4;

		saveBoilerSettings(true);
		ReInitBoiler();
		return;
	}

	if (strcmp(topic, "chac/ts/settime2") == 0)
	{
		char* data = (char*)payload;
		long tm = readHexInt32(data);

		setTime(tm + 4L * SECS_PER_HOUR);
		//RTC.set(now());
		printDateTime(&Serial, now());
		Serial.println();
		return;
	}

	if (strcmp(topic, "chac/ts/settime") == 0)
	{
		char* data = (char*)payload;
		int yr, month, day;
		int hr, min, sec;

		yr = 2000 + (*data++ - '0') * 10;
		yr += (*data++ - '0');

		month = (*data++ - '0') * 10;
		month += (*data++ - '0');

		day = (*data++ - '0') * 10;
		day += (*data++ - '0');

		data++;

		hr = (*data++ - '0') * 10;
		hr += (*data++ - '0');
		min = (*data++ - '0') * 10;
		min += (*data++ - '0');
		sec = (*data++ - '0') * 10;
		sec += (*data++ - '0');

		setTime(hr, min, sec, day, month, yr);
		//RTC.set(now());
		printDateTime(&Serial, now());
		return;
	}

	if (strcmp(topic, "chac/ts/mode") == 0)
	{
		boilerSettings.Mode = payload[0];
		saveBoilerSettings(true);
		ReInitBoiler();
		return;
	}
}

void publishTempSensorData()
{
	const char* topic = "cha/ts/sensors";

	int idx = 0;
	for (byte i = 0; i < ds18b20SensorCount; i++)
	{
		sprintf(buffer + idx, "#%d. id=%d, T=%d\n", i + 1, i, getTemperatureById(i));
		idx = strlen(buffer);
	}

	PublishMqtt(topic, buffer, idx, false);

	idx = 0;
	for (byte i = 0; i < MAX_DS1820_SENSORS; i++)
	{
		sprintf(buffer + idx, "#%d. id=%d, T=%d\n", i + 1, tempSensors[i].oneWireId, getTemperatureById(i));
		idx = strlen(buffer);
	}

	PublishMqtt(topic, buffer, idx, false);
}