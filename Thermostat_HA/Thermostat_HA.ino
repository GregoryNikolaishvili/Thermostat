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

#include <ArduinoHA.h>

#define REQUIRESALARMS false  // FOR DS18B20 library

#include "Thermostat.h"
#include "HASwitchX.h"

//#include <SPI.h>
#include <Ethernet.h>  // https://github.com/arduino-libraries/Ethernet
#include <MovingAverageFilter.h>

#include <Adafruit_MAX31865.h>  // https://github.com/adafruit/Adafruit_MAX31865
#include <OneWire.h>            // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <Temperature.h>
#include <TimeLib.h>     // https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>  // https://github.com/PaulStoffregen/TimeAlarms
//#include <DS1307RTC.h>				// https://github.com/PaulStoffregen/DS1307RTC

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

BoilerSettingStructure boilerSettings;

byte heaterRelayState[HEATER_RELAY_COUNT];

bool poolPumpIsOn;

byte boilerRelayPins[BOILER_RELAY_COUNT] = {
  PIN_BL_SOLAR_PUMP,
  PIN_BL_CIRC_PUMP,
  PIN_BL_OVEN_PUMP,
  PIN_BL_OVEN_FAN
};

// LED and timer variables
unsigned long halfSecondTicks = 0;
unsigned long secondTicks = 0;

EthernetClient client;
HADevice device;
HAMqtt mqtt(client, device, DEVICE_COUNT);

// HASwitchX instances
HASwitchX solarRecirculatingPump("pump_solar", "Solar pump", PIN_BL_SOLAR_PUMP);

// Home Assistant sensors
HASensorNumber pressureSensor("pressure_sensor", HASensorNumber::PrecisionP1);
HASensorNumber solarTemperatureSensor("solar_temperature", HASensorNumber::PrecisionP0);

void setup() {
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Initializing.. ver. 3.1.2"));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW);  // Turn on led at start

	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH);  // Disable SD card

	pinMode(PIN_ETHERNET_SS, OUTPUT);
	digitalWrite(PIN_ETHERNET_SS, HIGH);  // Disable Ethernet

	pinMode(PIN_MAX31865_SELECT, OUTPUT);
	digitalWrite(PIN_MAX31865_SELECT, HIGH);  // Disable MAX31865

	// Init relays
	for (byte i = 0; i < HEATER_RELAY_COUNT; i++) {
		heaterRelayState[i] = 100;  // Open all thermo valves to 100% (NO)
		digitalWrite(heaterRelayPins[i], LOW);
		pinMode(heaterRelayPins[i], OUTPUT);
	}

	for (byte i = 0; i < BOILER_RELAY_COUNT; i++) {
		digitalWrite(boilerRelayPins[i], HIGH);
		pinMode(boilerRelayPins[i], OUTPUT);
	}

	pinMode(PIN_PRESSURE_SENSOR, INPUT);

	// if (dtNBR_ALARMS != 30)
	// 	Serial.println(F("Alarm count mismatch"));

	setTime(0, 0, 1, 1, 1, 2001);

	initTemperatureSensors();
	initHelioPressureSensor();

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

void loop() {
	static unsigned long previousMillis = 0;  // will store last time LED was updated
	unsigned long _current_millis = millis();

	uint32_t dt = previousMillis > _current_millis ? 1 + previousMillis + ~_current_millis : _current_millis - previousMillis;

	if (dt >= 500) {
		wdt_reset();

		// save the last time we blinked the LED
		previousMillis = _current_millis;
		oncePerHalfSecond();
	}

	mqtt.loop();
	Alarm.delay(0);
}

void oncePerHalfSecond(void) {
	halfSecondTicks++;

	// Blinking
	static uint8_t blinkingLedState = LOW;

	blinkingLedState = ~blinkingLedState;
	digitalWrite(PIN_BLINKING_LED, blinkingLedState);

	if ((halfSecondTicks + 4) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0)  // 2 second before processing temperatures
		startDS18B20TemperatureMeasurements();

	if (halfSecondTicks % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) {
		bool publishError = false;

		ProcessTemperatureSensors(publishError);
		ProcessHelioPressure(publishError);

		if (publishError)
			PublishBoilerSettings();
	}

	if ((halfSecondTicks % 2) == 0)
		oncePerSecond();
}

void oncePerSecond() {
	processHeaterRelays();

	if ((secondTicks % 5) == 0)
		oncePer5Second();

	if ((secondTicks % 60) == 0)
		oncePer1Minute();

	secondTicks++;
}

static void oncePer5Second() {
}

static void oncePer1Minute() {
}

static void heaterRelaySetValue(byte id, byte value) {
	if (id < HEATER_RELAY_COUNT) {
		if (heaterRelayState[id] != value) {
			heaterRelayState[id] = value;

			//PublishHeaterRelayState(id, value);
		}
	}
}

static byte heaterRelayGetValue(byte id) {
	if (id < HEATER_RELAY_COUNT) {
		return heaterRelayState[id];
	}
	return 0;
}

static void processHeaterRelays() {
	for (byte id = 0; id < HEATER_RELAY_COUNT; id++) {
		if (heaterRelayState[id]) {
			digitalWrite(heaterRelayPins[id], LOW);  // OPEN
		}
		else {
			digitalWrite(heaterRelayPins[id], HIGH);  // CLOSE
		}
	}
}

static void solarPumpOn() {
	_boilerRelayOn(BL_SOLAR_PUMP);
}

void solarPumpOff() {
	_boilerRelayOff(BL_SOLAR_PUMP);
}

boolean isSolarPumpOn() {
	return _isBoilerRelayOn(BL_SOLAR_PUMP);
}

AlarmID_t circPumpStartingAlarm = 0xFF;

boolean circPumpStarting() {
	return circPumpStartingAlarm < 0xFF;
}

static void circPumpPumpOn() {
	if (circPumpStarting() || _isBoilerRelayOn(BL_CIRC_PUMP))
		return;

	if (boilerSettings.Mode == BOILER_MODE_WINTER)  // needs 5 min delay
	{
		circPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, circPumpOnTimer, "", 0);

		PublishBoilerRelayState(BL_CIRC_PUMP, false);  // will publish standby mode
	}
	else {
		_boilerRelayOn(BL_CIRC_PUMP);
	}
}

static void circPumpOnTimer(int tag, int tag2) {
	if (circPumpStartingAlarm < 0xFF) {
		Alarm.free(circPumpStartingAlarm);
		circPumpStartingAlarm = 0xFF;
	}
	_boilerRelayOn(BL_CIRC_PUMP);
}

static void circPumpPumpOff() {
	if (circPumpStartingAlarm < 0xFF) {
		Alarm.free(circPumpStartingAlarm);
		circPumpStartingAlarm = 0xFF;
		PublishBoilerRelayState(BL_CIRC_PUMP, false);
	}
	else {
		_boilerRelayOff(BL_CIRC_PUMP);
	}
}

static void burnerOn()  //TODO: never used
{
	_boilerRelayOn(BL_FURNACE);
}

static void burnerOff() {
	_boilerRelayOff(BL_FURNACE);
}

static void _boilerRelaySet(byte id, bool state) {
	if (state)
		_boilerRelayOn(id);
	else
		_boilerRelayOff(id);
}

static void _boilerRelayOn(byte id) {
	if (_isBoilerRelayOn(id))
		return;

	if (id < BOILER_RELAY_COUNT) {
		digitalWrite(boilerRelayPins[id], LOW);
		PublishBoilerRelayState(id, true);
	}
}

static void _boilerRelayOff(byte id) {
	if (!_isBoilerRelayOn(id))
		return;

	if (id < BOILER_RELAY_COUNT) {
		digitalWrite(boilerRelayPins[id], HIGH);
		PublishBoilerRelayState(id, false);
	}
}

static bool _isBoilerRelayOn(byte id) {
	if (id < BOILER_RELAY_COUNT)
		return !digitalRead(boilerRelayPins[id]);
	return false;
}

static void processPoolPumpState(byte id, bool value) {
	if (id != 0)
		return;

	poolPumpIsOn = value;

	bool publishError = false;
	ProcessTemperatureSensors(publishError);
}
