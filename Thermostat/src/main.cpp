// ქვედა რიგი მარცხნიდან მარჯვნივ(სულ 8)
//
// 1. სამზარეულო
// 2. ზალა 1 სამზარეულოსკენ როა
// 3. ზალა 1 ეზოსკენ რომ იყურება
// 4. კიბე ქვედა
// 5. ქვედა ტუალეტი
// 6. ზალა 1 კუთხეში
// 7.  ბარის შესასვლელი
// 8. ბარის ტუალეტი
//
// ზედა რიგი მარცხნიდან მარჯვნივ(სულ 12)
//
// 1. ზედა კიბეები
// 2. ზედა ტუალეტის საშრობი
// 3. ზედა ტუალეტის რადიატორი
// 4. ზალა 2 დიდი
// 5. ზალა 2 პატარა
// 6. გიო 1 და 2
// 7. ნანა
// 8. გიო 3
// 9. გია
// 10. ბარი(ბასეინისკენ)
// 11. კინოთეატრი ქვედა ეზოსკენ
// 12.  კინოთეატრი უკნისკენ(ფიჭვი)

#include <ProjectDefines.h>
#include "main.h"
#include <EEPROM.h>
#include <SPI.h>
#include "network.h"
#include "pressure_reader.h"

#include <ArduinoHA.h>

#include <HASwitchX.h>
#include <HATargetTemperatureX.h>
#include <HATargetTemperaturePoolX.h>
#include <HASettingX.h>
#include <HAModeX.h>
#include <HAErrorStatusX.h>

#include <TimeLib.h>	// https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h> // https://github.com/PaulStoffregen/TimeAlarms

#if defined(__AVR_ATmega2560__)
#include <avr/wdt.h>
#endif

#if defined(ESP8266) || defined(ESP32)
#include <EEPROM.h>
#endif

#include "processor.h"

// LED and timer variables
unsigned long halfSecondTicks = 0;
unsigned long secondTicks = 0;

#ifndef SIMULATION_MODE
EthernetClient client;
#else
WiFiClient client;
#endif
HADevice device;
HAMqtt mqtt(client, device, DEVICE_COUNT);

SolarTemperatureReader *solarT;

HAModeX *controllerMode;

// Home Assistant sensors

HASensorNumber *pressureSensor;
HASensorNumber *solarTemperatureSensor;
HASensorNumber *tankTopTemperatureSensor;
HASensorNumber *tankBottomTemperatureSensor;
HASensorNumber *roomTemperatureSensor;

HAErrorStatusX *errorStatus;

// Declare pointers for HASwitchX instances
HASwitchX *solarRecirculatingPump;
HASwitchX *heatingRecirculatingPump;

// Declare heating relay pointers
HASwitchX *heatingRelayStairs2;
HASwitchX *heatingRelayWc2;
HASwitchX *heatingRelayHall2;
HASwitchX *heatingRelayGio;
HASwitchX *heatingRelayNana;
HASwitchX *heatingRelayGio3;
HASwitchX *heatingRelayGia;
HASwitchX *heatingRelayBar;
HASwitchX *heatingRelayHT;
HASwitchX *heatingRelayKitchen;
HASwitchX *heatingRelayHall1;
HASwitchX *heatingRelayStairs1;
HASwitchX *heatingRelayWc1;

// Declare HATargetTemperatureX pointers for each room
HATargetTemperatureX *targetTemperatureBar;
HATargetTemperatureX *targetTemperatureHT;
HATargetTemperatureX *targetTemperatureGio;
HATargetTemperatureX *targetTemperatureGio3;
HATargetTemperatureX *targetTemperatureKitchen;
HATargetTemperatureX *targetTemperatureHall1;
HATargetTemperatureX *targetTemperatureStairs1;
HATargetTemperatureX *targetTemperatureWC1;
HATargetTemperatureX *targetTemperatureStairs2;
HATargetTemperatureX *targetTemperatureWC2;
HATargetTemperatureX *targetTemperatureGia;
HATargetTemperatureX *targetTemperatureNana;
HATargetTemperatureX *targetTemperatureHall2;
HATargetTemperaturePoolX *targetTemperaturePool;

RoomTemperatureSensor *roomSensors[MQTT_TEMPERATURE_SENSOR_COUNT];

// Declare pointers for HASettingX instances
HASettingX *settingCollectorSwitchOnTempDiff;
HASettingX *settingCollectorSwitchOffTempDiff;
HASettingX *settingCollectorEmergencySwitchOffT; // EMOF
HASettingX *settingCollectorEmergencySwitchOnT;	 // EMON
HASettingX *settingCollectorAntifreezeT;		 // CFR
HASettingX *settingMaxTankT;					 // SMX
HASettingX *settingAbsoluteMaxTankT;			 // ABS

HASettingX *settingPoolSwitchOnT;
HASettingX *settingPoolSwitchOffT;

extern bool warning_MAXT_IsActivated;

PressureReader *pressure;
TemperatureDS18B20 tankTemperatures(PIN_ONE_WIRE_BUS, STORAGE_ADDRESS_DS18B20_SETTINGS);

const char *pool_pump_switch_topic = "home/switch/pool_pump";

String pool_pump_state = "off";

// Forward declarations
void onMessageReceived(const char *topic, const uint8_t *payload, uint16_t length);
void onConnected();
void onControllerModeCommand(int8_t index, HASelect *sender);

void createHaObjects()
{
	controllerMode = new HAModeX(EEPROM_ADDR_CONTROLLER_MODE);
	controllerMode->onCommand(onControllerModeCommand);

	// Instantiate HASwitchX objects using new()
	solarRecirculatingPump = new HASwitchX("solar_pump", "Solar pump", PIN_BL_SOLAR_PUMP, true, false);
	heatingRecirculatingPump = new HASwitchX("heating_pump", "Heating pump", PIN_BL_CIRC_PUMP, true, false);

	pressureSensor = new HASensorNumber("solar_pressure_sensor", HASensorNumber::PrecisionP1);
	pressureSensor->setName("Solar pressure");
	pressureSensor->setUnitOfMeasurement("bar");
	pressureSensor->setIcon("mdi:gauge");
	pressureSensor->setDeviceClass("pressure");
	pressureSensor->setStateClass("measurement");

	pressure = new PressureReader(pressureSensor);

	// Instantiate HASensorNumber objects using new()
	solarTemperatureSensor = new HASensorNumber("solar_temperature", HASensorNumber::PrecisionP0);
	tankTopTemperatureSensor = new HASensorNumber("tank_top_temperature", HASensorNumber::PrecisionP0);
	tankBottomTemperatureSensor = new HASensorNumber("tank_bottom_temperature", HASensorNumber::PrecisionP0);
	roomTemperatureSensor = new HASensorNumber("room_temperature", HASensorNumber::PrecisionP0);

	solarTemperatureSensor->setName("Solar panel temperature");
	solarTemperatureSensor->setUnitOfMeasurement("°C");
	solarTemperatureSensor->setIcon("mdi:solar-panel-large");
	solarTemperatureSensor->setDeviceClass("temperature");
	solarTemperatureSensor->setStateClass("measurement");

	tankTopTemperatureSensor->setName("Tank top temperature");
	tankTopTemperatureSensor->setUnitOfMeasurement("°C");
	tankTopTemperatureSensor->setIcon("mdi:propane-tank");
	tankTopTemperatureSensor->setDeviceClass("temperature");
	tankTopTemperatureSensor->setStateClass("measurement");

	tankBottomTemperatureSensor->setName("Tank bottom temperature");
	tankBottomTemperatureSensor->setUnitOfMeasurement("°C");
	tankBottomTemperatureSensor->setIcon("mdi:propane-tank");
	tankBottomTemperatureSensor->setDeviceClass("temperature");
	tankBottomTemperatureSensor->setStateClass("measurement");

	roomTemperatureSensor->setName("Room temperature");
	roomTemperatureSensor->setUnitOfMeasurement("°C");
	roomTemperatureSensor->setDeviceClass("temperature");
	roomTemperatureSensor->setStateClass("measurement");

	// Instantiate HASensor objects using new()
	errorStatus = new HAErrorStatusX();

	// Instantiate heating relay objects using new()
	heatingRelayStairs2 = new HASwitchX("heating_relay_stairs_2", "ზედა კიბე", PIN_HR_1, false, false);
	heatingRelayWc2 = new HASwitchX("heating_relay_wc_2", "ზედა WC", PIN_HR_2, false, false);
	heatingRelayHall2 = new HASwitchX("heating_relay_hall_2", "ზალა 2", PIN_HR_3, false, false);
	heatingRelayGio = new HASwitchX("heating_relay_gio", "გიო", PIN_HR_4, false, false);
	heatingRelayNana = new HASwitchX("heating_relay_nana", "ნანა", PIN_HR_5, false, true);
	heatingRelayGio3 = new HASwitchX("heating_relay_gio3", "გიო 3", PIN_HR_6, false, false);
	heatingRelayGia = new HASwitchX("heating_relay_gia", "გია", PIN_HR_7, false, false);
	heatingRelayBar = new HASwitchX("heating_relay_bar", "ბარი", PIN_HR_8, false, false);
	heatingRelayHT = new HASwitchX("heating_relay_ht", "კინო", PIN_HR_9, false, false);
	heatingRelayKitchen = new HASwitchX("heating_relay_kitchen", "სამზარეულო", PIN_HR_10, false, false);
	heatingRelayHall1 = new HASwitchX("heating_relay_hall_1", "ზალა 1", PIN_HR_11, false, false);
	heatingRelayStairs1 = new HASwitchX("heating_relay_stairs_1", "ქვედა კიბე", PIN_HR_12, false, false);
	heatingRelayWc1 = new HASwitchX("heating_relay_wc_1", "ქვედა WC", PIN_HR_13, false, false);

	// Instantiate HATargetTemperatureX objects using new()
	targetTemperatureBar = new HATargetTemperatureX("target_temperature_bar", "ბარი", EEPROM_ADDR_TARGET_TEMPERATURE_BAR);
	targetTemperatureHT = new HATargetTemperatureX("target_temperature_ht", "კინო", EEPROM_ADDR_TARGET_TEMPERATURE_HT);
	targetTemperatureGio = new HATargetTemperatureX("target_temperature_gio", "გიო", EEPROM_ADDR_TARGET_TEMPERATURE_GIO);
	targetTemperatureGio3 = new HATargetTemperatureX("target_temperature_gio3", "გიო 3", EEPROM_ADDR_TARGET_TEMPERATURE_GIO3);
	targetTemperatureKitchen = new HATargetTemperatureX("target_temperature_kitchen", "სამზარეულო", EEPROM_ADDR_TARGET_TEMPERATURE_KITCHEN);
	targetTemperatureHall1 = new HATargetTemperatureX("target_temperature_hall_1", "ზალა 1", EEPROM_ADDR_TARGET_TEMPERATURE_HALL1);
	targetTemperatureStairs1 = new HATargetTemperatureX("target_temperature_stairs_1", "ქვედა კიბე", EEPROM_ADDR_TARGET_TEMPERATURE_STAIRS1);
	targetTemperatureWC1 = new HATargetTemperatureX("target_temperature_wc_1", "ქვედა WC", EEPROM_ADDR_TARGET_TEMPERATURE_WC1);
	targetTemperatureStairs2 = new HATargetTemperatureX("target_temperature_stairs_2", "ზედა კიბე", EEPROM_ADDR_TARGET_TEMPERATURE_STAIRS2);
	targetTemperatureWC2 = new HATargetTemperatureX("target_temperature_wc_2", "ზედა WC", EEPROM_ADDR_TARGET_TEMPERATURE_WC2);
	targetTemperatureGia = new HATargetTemperatureX("target_temperature_gia", "გია", EEPROM_ADDR_TARGET_TEMPERATURE_GIA);
	targetTemperatureNana = new HATargetTemperatureX("target_temperature_nana", "ნანა", EEPROM_ADDR_TARGET_TEMPERATURE_NANA);
	targetTemperatureHall2 = new HATargetTemperatureX("target_temperature_hall_2", "ზალა 2", EEPROM_ADDR_TARGET_TEMPERATURE_HALL2);
	targetTemperaturePool = new HATargetTemperaturePoolX("target_temperature_pool", "Pool", EEPROM_ADDR_TARGET_TEMPERATURE_POOL);

	// Create RoomTemperatureSensor objects using new() for each sensor
	roomSensors[0] = new RoomTemperatureSensor{"home/temperature/gw2000b_indoor_temperature", 0.0, targetTemperatureBar, heatingRelayBar};
	roomSensors[1] = new RoomTemperatureSensor{"home/temperature/gw2000b_temperature_1", 0.0, targetTemperatureHT, heatingRelayHT};
	roomSensors[2] = new RoomTemperatureSensor{"home/temperature/gw2000b_temperature_3", 0.0, targetTemperatureGio, heatingRelayGio};
	roomSensors[3] = new RoomTemperatureSensor{"home/temperature/gw2000b_temperature_4", 0.0, targetTemperatureGio3, heatingRelayGio3};

	roomSensors[4] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_1", 0.0, targetTemperatureKitchen, heatingRelayKitchen};
	roomSensors[5] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_2", 0.0, targetTemperatureHall1, heatingRelayHall1};
	roomSensors[6] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_3", 0.0, targetTemperatureStairs1, heatingRelayStairs1};
	roomSensors[7] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_4", 0.0, targetTemperatureWC1, heatingRelayWc1};
	roomSensors[8] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_5", 0.0, targetTemperatureStairs2, heatingRelayStairs2};
	roomSensors[9] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_6", 0.0, targetTemperatureWC2, heatingRelayWc2};
	roomSensors[10] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_7", 0.0, targetTemperatureGia, heatingRelayGia};
	roomSensors[11] = new RoomTemperatureSensor{"home/temperature/gw2000a_temperature_8", 0.0, targetTemperatureNana, heatingRelayNana};
	roomSensors[12] = new RoomTemperatureSensor{"home/temperature/gw2000a_indoor_temperature", 0.0, targetTemperatureHall2, heatingRelayHall2};

	roomSensors[13] = new RoomTemperatureSensor{"home/temperature/gw2000b_temperature_8", 0.0, targetTemperaturePool, NULL};
	roomSensors[14] = new RoomTemperatureSensor{"home/temperature/gw2000b_outdoor_temperature", 0.0, NULL, NULL};

	// 1. Collector Switch-On Temperature Difference (ΔT On)
	// The minimum temperature difference between the collector and the storage tank required to activate the circulation pump.
	// Optimal Value: 6°C
	// Range: 4°C (min) to 15°C (max) to allow flexibility for different operating conditions.
	settingCollectorSwitchOnTempDiff = new HASettingX("collector_switch_on_temp_diff", "Collector Switch-On Temperature Difference", 6, 4, 15, EEPROM_ADDR_COLLECTOR_SWITCH_ON_TEMP_DIFF);

	// 2. Collector Switch-Off Temperature Difference (ΔT Off)
	// The temperature difference at which the circulation pump deactivates to prevent inefficient operation.
	// Optimal Value: 3°C
	// Range: 2°C (min) to 6°C (max) to prevent short cycling and maintain efficiency.
	settingCollectorSwitchOffTempDiff = new HASettingX("collector_switch_off_temp_diff", "Collector Switch-Off Temperature Difference", 3, 2, 6, EEPROM_ADDR_COLLECTOR_SWITCH_OFF_TEMP_DIFF);

	// 3. Collector Emergency Switch-Off Temperature (EMOF)
	// The maximum allowable temperature for the collector before the system shuts down to prevent damage.
	// Optimal Value: 130°C
	// Range: 120°C (min) to 140°C (max) to ensure safety without premature shutdowns.
	settingCollectorEmergencySwitchOffT = new HASettingX("collector_emergency_switch_off_t", "Collector Emergency Switch-Off Temperature", 130, 120, 140, EEPROM_ADDR_COLLECTOR_EMERGENCY_SWITCH_OFF_T);

	// 4. Collector Emergency Switch-On Temperature (EMON)
	// The temperature at which the system can resume operation after an emergency shutdown.
	// Optimal Value: 110°C
	// Adjusted Range: 100°C (min) to 125°C (max) to ensure safe resumption and prevent rapid cycling.
	settingCollectorEmergencySwitchOnT = new HASettingX("collector_emergency_switch_on_t", "Collector Emergency Switch-On Temperature", 110, 100, 125, EEPROM_ADDR_COLLECTOR_EMERGENCY_SWITCH_ON_T);

	// 5. Collector Antifreeze Temperature (CFR)
	// The temperature that activates the antifreeze function to prevent freezing of the collector and pipes.
	// Optimal Value: 2°C
	// Adjusted Range: 0°C (min) to 5°C (max) to balance freeze protection and energy efficiency.
	settingCollectorAntifreezeT = new HASettingX("collector_antifreeze_t", "Collector Antifreeze Temperature", 2, 0, 5, EEPROM_ADDR_COLLECTOR_ANTIFREEZE_T);

	// 6. Maximum Tank Temperature (SMX)
	// The maximum temperature allowed for the storage tank to prevent overheating and potential damage.
	// Optimal Value: 75°C
	// Adjusted Range: 65°C (min) to 85°C (max) to allow for varying hot water needs and safety margins.
	settingMaxTankT = new HASettingX("max_tank_temp", "Maximum Tank Temperature", 75, 65, 85, EEPROM_ADDR_MAX_TANK_TEMP);

	// 7. Absolute Maximum Tank Temperature (ABS)
	// An emergency limit to prevent the tank from reaching dangerous temperatures.
	// Optimal Value: 85°C
	// Adjusted Range: 80°C (min) to 90°C (max) to provide a safety buffer for system protection.
	settingAbsoluteMaxTankT = new HASettingX("absolute_max_tank_temp", "Absolute Maximum Tank Temperature", 85, 80, 90, EEPROM_ADDR_ABSOLUTE_MAX_TANK_TEMP);

	// 8. Tank Temperature to Switch On Pool Heating
	// The tank temperature at which the pool heating system switches on.
	// Optimal Value: 55°C
	// Range: 40°C (min) to 70°C (max) to allow for different user preferences.
	settingPoolSwitchOnT = new HASettingX("pool_switch_on_t", "Tank Temperature to Switch On Pool Heating", 55, 40, 70, EEPROM_ADDR_POOL_SWITCH_ON_T);

	// 9. Tank Temperature to Switch Off Pool Heating
	// The tank temperature at which the pool heating system switches off.
	// Optimal Value: 50°C
	// Range: 35°C (min) to 65°C (max) to allow for different user preferences.
	settingPoolSwitchOffT = new HASettingX("pool_switch_off_t", "Tank Temperature to Switch Off Pool Heating", 50, 35, 65, EEPROM_ADDR_POOL_SWITCH_OFF_T);

#ifndef SIMULATION_MODE
	device.setName("Thermostat controller");
#else
	device.setName("Thermostat controller (Simulated)");
#endif
	device.setSoftwareVersion(CONTROLLER__VERSION);
	device.setManufacturer("Gregory Nikolaishvili");
}

void initHaObjects()
{
	device.enableSharedAvailability();
	device.enableLastWill();

	solarRecirculatingPump->setState(true);

	// heatingRelayStairs2->setDefaultState();
	// heatingRelayWc2->setDefaultState();
	// heatingRelayHall2->setDefaultState();
	// heatingRelayGio->setDefaultState();
	// heatingRelayNana->setDefaultState();
	// heatingRelayGio3->setDefaultState();
	// heatingRelayGia->setDefaultState();
	// heatingRelayBar->setDefaultState();
	// heatingRelayHT->setDefaultState();
	// heatingRelayKitchen->setDefaultState();
	// heatingRelayHall1->setDefaultState();
	// heatingRelayStairs1->setDefaultState();
	// heatingRelayWc1->setDefaultState();
}

void setup()
{
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.print(F("Initializing.. ver. "));
	Serial.println(F(CONTROLLER__VERSION));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW); // Turn on led at start

#if defined(ESP8266) || defined(ESP32)
	// Initialize EEPROM for ESP devices
	EEPROM.begin(128); // Adjust the size as needed
#endif

#ifndef SIMULATION_MODE
	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH); // Disable SD card

	pinMode(PIN_ETHERNET_SS, OUTPUT);
	digitalWrite(PIN_ETHERNET_SS, HIGH); // Disable Ethernet
#endif

	int eepromSavedVersion = 0;
	EEPROM.get(EEPROM_ADDR_VERSION, eepromSavedVersion);
	if (eepromSavedVersion != EEPROM_CURRENT_VERSION)
	{
		for (int i = 0; i < STORAGE_ADDRESS_DS18B20_SETTINGS; i++)
			EEPROM.write(i, 0xFF); // Write 0xFF to each byte (erased state)
	}

	solarT = new SolarTemperatureReader(PIN_MAX31865_SELECT);

	tankTemperatures.initialize();

	setTime(0, 0, 1, 1, 1, 2001);

	createHaObjects();

	delay(1000);

#ifndef SIMULATION_MODE
	initNetwork(device, client);
#else
	initNetwork(device);
#endif

	delay(500);

	initHaObjects();

	mqtt.begin(MQTT_BROKER, MQTT_USERNAME, MQTT_PASSWORD);
	mqtt.onConnected(onConnected);
	mqtt.onMessage(onMessageReceived);

	wdt_enable(WDTO_8S);
	Serial.println(F("Start"));
}

void onConnected()
{
	Serial.println(F("Connected"));

	// Subscribe to temperature topics
	for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
	{
		if (roomSensors[i]->topic != NULL) // Ensure the topic is valid
		{
			mqtt.subscribe(roomSensors[i]->topic);
			Serial.print(F("Subscribed to "));
			Serial.println(roomSensors[i]->topic);
		}
	}

	// Subscribe to pool pump switch topic
	mqtt.subscribe(pool_pump_switch_topic);
	Serial.print(F("Subscribed to "));
	Serial.println(pool_pump_switch_topic);
}

void onMessageReceived(const char *topic, const uint8_t *payload, uint16_t length)
{
	// Convert payload from const uint8_t* to String
	String message = "";
	for (unsigned int i = 0; i < length; i++)
	{
		message += (char)payload[i];
	}

	// Serial.print(F("Message arrived ["));
	// Serial.print(topic);
	// Serial.print(F("]: "));
	// Serial.println(message);

	// Iterate through all sensors to find a matching topic
	for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
	{
		// Check if the topic matches the temperature sensor
		if (strcmp(topic, roomSensors[i]->topic) == 0)
		{
			// Convert payload to float
			float tempValue = message.toFloat();
			if (isnan(tempValue))
			{
				Serial.println(F("Error: Received invalid temperature value."));
				continue;
			}
			roomSensors[i]->value = tempValue;

			Serial.print(F("Updated "));
			Serial.print(roomSensors[i]->topic);
			Serial.print(F(" to "));
			Serial.println(tempValue);

			return; // Exit after handling
		}
	}

	// Handle pool pump switch Topic
	if (strcmp(topic, pool_pump_switch_topic) == 0)
	{
		pool_pump_state = message;

		Serial.print(F("Pool Pump State updated to: "));
		Serial.println(pool_pump_state);

		ProcessTemperatureSensors();
	}
}

void onControllerModeCommand(int8_t index, HASelect *sender)
{
	if (controllerMode->getCurrentState() != index)
	{
		controllerMode->SaveModeToEeprom(index);
		sender->setState(index); // report the selected option back to the HA panel

		initThermostat(index);
	}

	// it may return null
	if (sender->getCurrentOption())
	{
		Serial.print(F("Current option: "));
		Serial.println(sender->getCurrentOption());
	}
}

void processHeaterRelays()
{
	if (warning_MAXT_IsActivated || controllerMode->getCurrentState() != THERMOSTAT_MODE_WINTER)
		return;

	// Iterate through all sensors to find a relay
	for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
	{
		RoomTemperatureSensor *sensor = roomSensors[i];
		if (sensor->relay == NULL || sensor->targetTemperature == NULL)
			continue;

		float tempValue = sensor->value;
		float targetTemp = sensor->targetTemperature->getCurrentState().toFloat(); // Get current target temperature
		float hysteresis = 0.2f;												   // Define hysteresis value

		// Serial.print(F("Room: "));
		// Serial.println(sensor->topic);
		// Serial.print(F("T: "));
		// Serial.println(tempValue);
		// Serial.print(F("TT: "));
		// Serial.println(targetTemp);

		bool turnOn = false;

		if (targetTemp > 16.0f)
		{
			if (isnan(tempValue))
			{
				turnOn = true;
				Serial.println(F("Invalid room temperature"));
			}
			else if (tempValue < (targetTemp - hysteresis))
			{
				turnOn = true;
			}
			else if (tempValue > (targetTemp + hysteresis))
			{
				turnOn = false;
			}

			// Turn ON
			if (turnOn != sensor->relay->isTurnedOn())
			{
				sensor->relay->setOnOff(turnOn);
				Serial.print(F("Heating Relay "));
				Serial.print(i + 1);
				Serial.print(F(" turned "));
				Serial.println(turnOn);
			}
		}
	}
}

static void oncePer1Minute()
{
	processHeaterRelays();
}

static void oncePer5Second()
{
}

void oncePerSecond()
{
	// Serial.print(F("MQTT: "));
	// Serial.println(mqtt.isConnected());
	if ((secondTicks % 5) == 0)
		oncePer5Second();

	if ((secondTicks % 60) == 0)
		oncePer1Minute();

	secondTicks++;
}

void oncePerHalfSecond(void)
{
	halfSecondTicks++;

	// Blinking
	static uint8_t blinkingLedState = LOW;

	blinkingLedState = ~blinkingLedState;
	digitalWrite(PIN_BLINKING_LED, blinkingLedState);

#ifndef SIMULATION_MODE
	if ((halfSecondTicks + 4) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) // 2 second before processing temperatures
		tankTemperatures.startMeasurements();
#endif

	if (halfSecondTicks % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0)
	{
		pressure->processPressureSensor();
#ifndef SIMULATION_MODE
		ProcessTemperatureSensors();
#endif
	}

	if ((halfSecondTicks % 2) == 0)
		oncePerSecond();
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

	Alarm.delay(0);
#ifndef SIMULATION_MODE
	Ethernet.maintain();
#endif
	mqtt.loop();
}
