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

#include <ArduinoHA.h>

// #define REQUIRESALARMS false // FOR DS18B20 library

#include "main.h"
#include "network.h"
#include "pressure_reader.h"

#include <HASwitchX.h>
#include <HATargetTemperatureX.h>
#include <HATargetTemperaturePoolX.h>
#include <HASettingX.h>

#include <TimeLib.h>	// https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h> // https://github.com/PaulStoffregen/TimeAlarms

#if defined(__AVR_ATmega2560__)
#include <avr/wdt.h>
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

HASelect modeSelect("mode");

// HASwitchX instances
HASwitchX solarRecirculatingPump("solar_pump", "Solar pump", PIN_BL_SOLAR_PUMP, false);
HASwitchX heatingRecirculatingPump("heating_pump", "Heating pump", PIN_BL_CIRC_PUMP, false);

// Home Assistant sensors
HASensorNumber pressureSensor("solar_pressure_sensor", HASensorNumber::PrecisionP1);
HASensorNumber solarTemperatureSensor("solar_temperature", HASensorNumber::PrecisionP0);
HASensorNumber tankTopTemperatureSensor("tank_top_temperature", HASensorNumber::PrecisionP0);
HASensorNumber tankBottomTemperatureSensor("tank_bottom_temperature", HASensorNumber::PrecisionP0);
HASensorNumber roomTemperatureSensor("room_temperature", HASensorNumber::PrecisionP0);

HASensor warningStatus("warning_state", HASensor::JsonAttributesFeature);
HASensor errorStatus("error_state", HASensor::JsonAttributesFeature);

HASwitchX heatingRelayStairs2("heating_relay_stairs_2", "ზედა კიბე", PIN_HR_1, false);
HASwitchX heatingRelayWc2("heating_relay_wc_2", "ზედა WC", PIN_HR_2, false);
HASwitchX heatingRelayHall2("heating_relay_hall_2", "ზალა 2", PIN_HR_3, false);
HASwitchX heatingRelayGio("heating_relay_gio", "გიო", PIN_HR_4, false);
HASwitchX heatingRelayNana("heating_relay_nana", "ნანა", PIN_HR_5, false);
HASwitchX heatingRelayGio3("heating_relay_gio3", "გიო 3", PIN_HR_6, false);
HASwitchX heatingRelayGia("heating_relay_gia", "გია", PIN_HR_7, false);
HASwitchX heatingRelayBar("heating_relay_bar", "ბარი", PIN_HR_8, false);
HASwitchX heatingRelayHT("heating_relay_ht", "კინო", PIN_HR_9, false);
HASwitchX heatingRelayKitchen("heating_relay_kitchen", "სამზარეულო", PIN_HR_10, false);
HASwitchX heatingRelayHall1("heating_relay_hall_1", "ზალა 1", PIN_HR_11, false);
HASwitchX heatingRelayStairs1("heating_relay_stairs_1", "ქვედა კიბე", PIN_HR_12, false);
HASwitchX heatingRelayWc1("heating_relay_wc_1", "ქვედა WC", PIN_HR_13, false);

// Define HATargetTemperatureX instances for each room
HATargetTemperatureX targetTemperatureBar("target_temperature_bar", "ბარი");
HATargetTemperatureX targetTemperatureHT("target_temperature_ht", "კინო");
HATargetTemperatureX targetTemperatureGio("target_temperature_gio", "გიო");
HATargetTemperatureX targetTemperatureGio3("target_temperature_gio3", "გიო 3");
HATargetTemperatureX targetTemperatureKitchen("target_temperature_kitchen", "სამზარეულო");
HATargetTemperatureX targetTemperatureHall1("target_temperature_hall_1", "ზალა 1");
HATargetTemperatureX targetTemperatureKvetaStairs1("target_temperature_kveta_stairs_1", "ქვედა კიბე");
HATargetTemperatureX targetTemperatureWC1("target_temperature_wc_1", "ქვედა WC");
HATargetTemperatureX targetTemperatureStairs2("target_temperature_stairs_2", "ზედა კიბე");
HATargetTemperatureX targetTemperatureWC2("target_temperature_wc_2", "ზედა WC");
HATargetTemperatureX targetTemperatureGia("target_temperature_gia", "გია");
HATargetTemperatureX targetTemperatureNana("target_temperature_nana", "ნანა");
HATargetTemperatureX targetTemperatureHall2("target_temperature_hall_2", "ზალა 2");
HATargetTemperaturePoolX targetTemperaturePool("target_temperature_pool", "Pool");

// Initialize RoomTemperatureSensor Array
RoomTemperatureSensor roomSensors[] = {
	{"home/temperature/gw2000b_indoor_temperature", 0.0, &targetTemperatureBar, &heatingRelayBar},
	{"home/temperature/gw2000b_temperature_1", 0.0, &targetTemperatureHT, &heatingRelayHT},
	{"home/temperature/gw2000b_temperature_3", 0.0, &targetTemperatureGio, &heatingRelayGio},
	{"home/temperature/gw2000b_temperature_4", 0.0, &targetTemperatureGio3, &heatingRelayGio3},

	{"home/temperature/gw2000a_temperature_1", 0.0, &targetTemperatureKitchen, &heatingRelayKitchen},
	{"home/temperature/gw2000a_temperature_2", 0.0, &targetTemperatureHall1, &heatingRelayHall1},
	{"home/temperature/gw2000a_temperature_3", 0.0, &targetTemperatureKvetaStairs1, &heatingRelayStairs1},
	{"home/temperature/gw2000a_temperature_4", 0.0, &targetTemperatureWC1, &heatingRelayWc1},
	{"home/temperature/gw2000a_temperature_5", 0.0, &targetTemperatureStairs2, &heatingRelayStairs2},
	{"home/temperature/gw2000a_temperature_6", 0.0, &targetTemperatureWC2, &heatingRelayWc2},
	{"home/temperature/gw2000a_temperature_7", 0.0, &targetTemperatureGia, &heatingRelayGia},
	{"home/temperature/gw2000a_temperature_8", 0.0, &targetTemperatureNana, &heatingRelayNana},
	{"home/temperature/gw2000a_indoor_temperature", 0.0, &targetTemperatureHall2, &heatingRelayHall2},

	{"home/temperature/gw2000b_temperature_8", 0.0, &targetTemperaturePool, NULL},
	{"home/temperature/gw2000b_outdoor_temperature", 0.0, NULL, NULL}};

// Number of temperature sensors
int MQTT_TEMPERATURE_SENSOR_COUNT = sizeof(roomSensors) / sizeof(roomSensors[0]);

// Declare instances of HASettingX for each setting
HASettingX settingCollectorSwitchOnTempDiff("collector_switch_on_temp_diff", "Collector Switch-On Temperature Difference", 8, 2, 20);
HASettingX settingCollectorSwitchOffTempDiff("collector_switch_off_temp_diff", "Collector Switch-Off Temperature Difference", 4, 2, 10);
HASettingX settingCollectorEmergencySwitchOffT("collector_emergency_switch_off_t", "Collector Emergency Switch-Off Temperature", 140, 110, 150); // EMOF
HASettingX settingCollectorEmergencySwitchOnT("collector_emergency_switch_on_t", "Collector Emergency Switch-On Temperature", 120, 103, 147);	 // EMON, Max set to (EMOF - 3)
HASettingX settingCollectorMinimumSwitchOnT("collector_minimum_switch_on_t", "Collector Minimum Switch-On Temperature", 10, -1, 9);				 // CMN
HASettingX settingCollectorAntifreezeT("collector_antifreeze_t", "Collector Antifreeze Temperature", 4, -1, 6);									 // CFR
HASettingX settingMaxTankT("max_tank_temp", "Maximum Tank Temperature", 70, 60, 95);															 // SMX
HASettingX settingAbsoluteMaxTankT("absolute_max_tank_temp", "Absolute Maximum Tank Temperature", 90, 80, 95);									 // ABS

HASettingX settingPoolSwitchOnT("pool_switch_on_t", "Tank Temperature to Switch On Pool Heating", 55, 40, 70);
HASettingX settingPoolSwitchOffT("pool_switch_off_t", "Tank Temperature to Switch Off Pool Heating", 50, 35, 65);

const char *pool_pump_switch_topic = "home/switch/pool_pump";

// Variable to store pool pump switch state
String pool_pump_state = "off";

PressureReader pressure = PressureReader(pressureSensor);
TemperatureDS18B20 tankTemperatures(PIN_ONE_WIRE_BUS, STORAGE_ADDRESS_BOILER_SETTINGS);

// Forward declarations
void onMessageReceived(const char *topic, const uint8_t *payload, uint16_t length);
void onConnected();
void onModeSelectCommand(int8_t index, HASelect *sender);

void setup()
{
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Initializing.. ver. 4.0.5"));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW); // Turn on led at start

#ifndef SIMULATION_MODE
	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH); // Disable SD card

	pinMode(PIN_ETHERNET_SS, OUTPUT);
	digitalWrite(PIN_ETHERNET_SS, HIGH); // Disable Ethernet
#endif

	setTime(0, 0, 1, 1, 1, 2001);

	delay(1000);

#ifndef SIMULATION_MODE
	initNetwork(device, client);
#else
	initNetwork(device);
#endif

	delay(500);

	mqtt.begin(MQTT_BROKER, MQTT_USERNAME, MQTT_PASSWORD);

	device.enableSharedAvailability();
	device.enableLastWill();

#ifndef SIMULATION_MODE
	device.setName("Thermostat controller");
#else
	device.setName("Thermostat controller (Simulated)");
#endif
	device.setSoftwareVersion("4.0.4");
	device.setManufacturer("Gregory Nikolaishvili");

	solarRecirculatingPump.setState(true);
	heatingRecirculatingPump.setState(true);

	modeSelect.setIcon("mdi:auto-mode");
	modeSelect.setName("Thermostat mode");
	modeSelect.setOptions("Off;Winter;Summer;Summer & Pool");
	modeSelect.setRetain(true);
	modeSelect.onCommand(onModeSelectCommand);

	pressureSensor.setName("Solar pressure");
	pressureSensor.setUnitOfMeasurement("bar");
	pressureSensor.setIcon("mdi:gauge");
	pressureSensor.setDeviceClass("pressure");
	pressureSensor.setStateClass("measurement");

	solarTemperatureSensor.setName("Solar panel temperature");
	solarTemperatureSensor.setUnitOfMeasurement("°C");
	solarTemperatureSensor.setIcon("mdi:solar-panel-large");
	solarTemperatureSensor.setDeviceClass("temperature");
	solarTemperatureSensor.setStateClass("measurement");

	tankTopTemperatureSensor.setName("Tank top temperature");
	tankTopTemperatureSensor.setUnitOfMeasurement("°C");
	tankTopTemperatureSensor.setIcon("mdi:propane-tank");
	tankTopTemperatureSensor.setDeviceClass("temperature");
	tankTopTemperatureSensor.setStateClass("measurement");

	tankBottomTemperatureSensor.setName("Tank bottom temperature");
	tankBottomTemperatureSensor.setUnitOfMeasurement("°C");
	tankBottomTemperatureSensor.setIcon("mdi:propane-tank");
	tankBottomTemperatureSensor.setDeviceClass("temperature");
	tankBottomTemperatureSensor.setStateClass("measurement");

	roomTemperatureSensor.setName("Room temperature");
	roomTemperatureSensor.setUnitOfMeasurement("°C");
	roomTemperatureSensor.setDeviceClass("temperature");
	roomTemperatureSensor.setStateClass("measurement");

	errorStatus.setName("Error state");
	errorStatus.setIcon("mdi:alert");

	heatingRelayStairs2.setState(true);
	heatingRelayWc2.setState(true);
	heatingRelayHall2.setState(true);
	heatingRelayGio.setState(true);
	heatingRelayNana.setState(true);
	heatingRelayGio3.setState(true);
	heatingRelayGia.setState(true);
	heatingRelayBar.setState(true);
	heatingRelayHT.setState(true);
	heatingRelayKitchen.setState(true);
	heatingRelayHall1.setState(true);
	heatingRelayStairs1.setState(true);
	heatingRelayWc1.setState(true);

	settingCollectorSwitchOnTempDiff.setInitialValue();
	settingCollectorSwitchOffTempDiff.setInitialValue();
	settingCollectorEmergencySwitchOffT.setInitialValue();
	settingCollectorEmergencySwitchOnT.setInitialValue();
	settingCollectorMinimumSwitchOnT.setInitialValue();
	settingCollectorAntifreezeT.setInitialValue();
	settingMaxTankT.setInitialValue();
	settingAbsoluteMaxTankT.setInitialValue();
	settingPoolSwitchOnT.setInitialValue();
	settingPoolSwitchOffT.setInitialValue();

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
		if (roomSensors[i].topic != NULL) // Ensure the topic is valid
		{
			mqtt.subscribe(roomSensors[i].topic);
			Serial.print(F("Subscribed to "));
			Serial.println(roomSensors[i].topic);
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

	Serial.print(F("Message arrived ["));
	Serial.print(topic);
	Serial.print(F("]: "));
	Serial.println(message);

	// Iterate through all sensors to find a matching topic
	for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
	{
		// Check if the topic matches the temperature sensor
		if (strcmp(topic, roomSensors[i].topic) == 0)
		{
			// Convert payload to float
			float tempValue = message.toFloat();
			if (isnan(tempValue))
			{
				Serial.println(F("Error: Received invalid temperature value."));
				continue;
			}
			roomSensors[i].value = tempValue;

			Serial.print(F("Updated "));
			Serial.print(roomSensors[i].topic);
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

void onModeSelectCommand(int8_t index, HASelect *sender)
{
	sender->setState(index); // report the selected option back to the HA panel

	initThermostat();

	// it may return null
	if (sender->getCurrentOption())
	{
		Serial.print("Current option: ");
		Serial.println(sender->getCurrentOption());
	}
}

void processHeaterRelays()
{
	// Iterate through all sensors to find a relay
	for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
	{
		RoomTemperatureSensor sensor = roomSensors[i];
		if (sensor.relay == NULL || sensor.targetTemperature == NULL)
			continue;

		float tempValue = sensor.targetTemperature->getTargetTemperature();
		if (isnan(tempValue))
		{
			// Turn ON
			if (!sensor.relay->isTurnedOn())
			{
				sensor.relay->setOnOff(HIGH);
				Serial.print(F("Heating Relay "));
				Serial.print(i + 1);
				Serial.println(F(" turned ON."));
			}
			continue;
		}

		float targetTemp = sensor.targetTemperature->getTargetTemperature(); // Get current target temperature
		float hysteresis = 0.5;													  // Define hysteresis value

		if (tempValue < (targetTemp - hysteresis))
		{
			if (!sensor.relay->isTurnedOn())
			{
				sensor.relay->setOnOff(HIGH);
				Serial.print(F("Heating Relay "));
				Serial.print(i + 1);
				Serial.println(F(" turned ON."));
			}
		}
		else if (tempValue > (targetTemp + hysteresis))
		{
			if (sensor.relay->isTurnedOn())
			{
				sensor.relay->setOnOff(LOW);
				Serial.print(F("Heating Relay "));
				Serial.print(i + 1);
				Serial.println(F(" turned OFF."));
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
	// Serial.print("MQTT: ");
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
		pressure.processPressureSensor();
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
	mqtt.loop();
}
