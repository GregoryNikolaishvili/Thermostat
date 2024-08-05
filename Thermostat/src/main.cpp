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

#include <TimeLib.h>	// https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h> // https://github.com/PaulStoffregen/TimeAlarms

#if defined(__AVR_ATmega2560__)
#include <avr/wdt.h>
#endif

#include "settings.h"
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

// HASwitchX instances
HASwitchX solarRecirculatingPump("solar_pump", "Solar pump", PIN_BL_SOLAR_PUMP, false);
HASwitchX heatingRecirculatingPump("heating_pump", "Heating pump", PIN_BL_CIRC_PUMP, false);

// Home Assistant sensors
HASensorNumber pressureSensor("solar_pressure_sensor", HASensorNumber::PrecisionP1);
HASensorNumber solarTemperatureSensor("solar_temperature", HASensorNumber::PrecisionP0);
HASensorNumber boilerTopTemperatureSensor("boiler_top_temperature", HASensorNumber::PrecisionP0);
HASensorNumber boilerBottomTemperatureSensor("boiler_bottom_temperature", HASensorNumber::PrecisionP0);
HASensorNumber boilerRoomTemperatureSensor("boiler_room_temperature", HASensorNumber::PrecisionP0);

HASensor errorStatus("error_state", HASensor::JsonAttributesFeature);

// HASwitchX heaterRelay();

PressureReader pressure = PressureReader(pressureSensor);
TemperatureDS18B20 boilerT(PIN_ONE_WIRE_BUS, STORAGE_ADDRESS_BOILER_SETTINGS);

void setup()
{
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Initializing.. ver. 4.0.4"));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW); // Turn on led at start

#ifndef SIMULATION_MODE
	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH); // Disable SD card

	pinMode(PIN_ETHERNET_SS, OUTPUT);
	digitalWrite(PIN_ETHERNET_SS, HIGH); // Disable Ethernet
#endif

	setTime(0, 0, 1, 1, 1, 2001);

#ifndef SIMULATION_MODE
	initHeaterRelays();
#endif

	readBoilerSettings();

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
	solarRecirculatingPump.setName("Solar recirculating pump");

	heatingRecirculatingPump.setState(true);
	heatingRecirculatingPump.setName("Heating recirculating pump");

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

	boilerTopTemperatureSensor.setName("Boiler top temperature");
	boilerTopTemperatureSensor.setUnitOfMeasurement("°C");
	boilerTopTemperatureSensor.setIcon("mdi:propane-tank");
	boilerTopTemperatureSensor.setDeviceClass("temperature");
	boilerTopTemperatureSensor.setStateClass("measurement");

	boilerBottomTemperatureSensor.setName("Boiler bottom temperature");
	boilerBottomTemperatureSensor.setUnitOfMeasurement("°C");
	boilerBottomTemperatureSensor.setIcon("mdi:propane-tank");
	boilerBottomTemperatureSensor.setDeviceClass("temperature");
	boilerBottomTemperatureSensor.setStateClass("measurement");

	boilerRoomTemperatureSensor.setName("Boiler room temperature");
	boilerRoomTemperatureSensor.setUnitOfMeasurement("°C");
	boilerRoomTemperatureSensor.setDeviceClass("temperature");
	boilerRoomTemperatureSensor.setStateClass("measurement");

	errorStatus.setName("Error state");
	errorStatus.setIcon("mdi:alert-circle");
	errorStatus.setJsonAttributes

	wdt_enable(WDTO_8S);
	Serial.println(F("Start"));
}

static void oncePer1Minute()
{
}

static void oncePer5Second()
{
}

void oncePerSecond()
{
	Serial.print("MQTT: ");
	Serial.println(mqtt.isConnected());
	// processHeaterRelays();

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
		boilerT.startMeasurements();
#endif

	if (halfSecondTicks % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0)
	{
		Serial.println("Pressure 1");
		pressure.processPressureSensor();

		Serial.println("Temperature 1");
		ProcessTemperatureSensors();
		// PublishBoilerSettings();
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

	mqtt.loop();
	Alarm.delay(0);
}
