#define REQUIRESALARMS false // FOR DS18B20 library
#define MQTT_MAX_PACKET_SIZE 300 // FOR PubSubClient library // 5 + 2 + strlen(topic) + plength
#define MQTT_SOCKET_TIMEOUT 5 // FOR PubSubClient library

#include "Thermostat.h"

#include <TimeLib.h>				// https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>				// https://github.com/PaulStoffregen/TimeAlarms
#include <DS1307RTC.h>				// https://github.com/PaulStoffregen/DS1307RTC
//#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>			// https://github.com/knolleary/pubsubclient
#include <Adafruit_MAX31865.h>		// https://github.com/adafruit/Adafruit_MAX31865
#include <OneWire.h>				// https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>	    // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <avr/wdt.h>

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

byte boilerRelayPins[BOILER_RELAY_COUNT] = {
	PIN_BL_SOLAR_PUMP,
	PIN_BL_CIRC_PUMP,
	PIN_BL_OVEN_PUMP,
	PIN_BL_OVEN_FAN
};

unsigned long halfSecondTicks = 0;
unsigned long secondTicks = 0;

unsigned int thermostatControllerState = 0;

uint16_t lastReadSolarPanelRTD;

void setup()
{
	wdt_disable();

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Initializing.. ver. 1.1.0"));

	pinMode(PIN_BLINKING_LED, OUTPUT);
	digitalWrite(PIN_BLINKING_LED, LOW); // Turn on led at start

	pinMode(PIN_MANUAL_MODE_LED, OUTPUT);
	digitalWrite(PIN_MANUAL_MODE_LED, LOW); // Turn on manual mode led at start

	//pinMode(PIN_MAX31865_SELECT, OUTPUT);
	//digitalWrite(PIN_MAX31865_SELECT, HIGH); // Disable MAX31865

	pinMode(PIN_SD_CARD_SELECT, OUTPUT);
	digitalWrite(PIN_SD_CARD_SELECT, HIGH); // Disable SD card

	// Init relays
	for (byte i = 0; i < HEATER_RELAY_COUNT; i++)
	{
		heaterRelayState[i] = 100;
		digitalWrite(heaterRelayPins[i], LOW);
		pinMode(heaterRelayPins[i], OUTPUT);
	}

	for (byte i = 0; i < BOILER_RELAY_COUNT; i++)
	{
		digitalWrite(boilerRelayPins[i], HIGH);
		pinMode(boilerRelayPins[i], OUTPUT);
	}

	if (dtNBR_ALARMS != 30)
		Serial.println(F("Alarm count mismatch"));

	SPI.begin();

	//Wire.begin();

	InitTemperatureSensors();

	readSettings();

	InitEthernet();

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

extern Adafruit_MAX31865 solarSensor;

void oncePerHalfSecond(void)
{
	halfSecondTicks++;

	// Blinking
	static uint8_t blinkingLedState = LOW;

	blinkingLedState = !blinkingLedState;
	digitalWrite(PIN_BLINKING_LED, blinkingLedState);

	if ((halfSecondTicks + 4) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) // 2 second before processing temperatures
		startDS18B20TemperatureMeasurements();
	if ((halfSecondTicks + 2) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) // 1 second before processing temperatures
		solarSensor.readRTD_step1();
	if ((halfSecondTicks + 1) % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0) // 0.5 second before processing temperatures
		solarSensor.readRTD_step2();

	if (halfSecondTicks % PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC == 0)
	{
		lastReadSolarPanelRTD = solarSensor.readRTD_step3();
		ProcessTemperatureSensors();
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
	ReconnectMqtt();
}

void oncePer1Minute()
{
	ProcessRoomSensors();

	if (secondTicks > 0) // do not publish on startup
		PublishAllStates(true);
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
	static unsigned long windowStartTime = 0;

	if (secondTicks - windowStartTime > 100) // time to shift the Relay Window
		windowStartTime += 100;

	for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
	{
		int output = heaterRelayState[id];

		if (output < (int)(secondTicks - windowStartTime))
		{
			digitalWrite(heaterRelayPins[id], HIGH);
		}
		else
		{
			digitalWrite(heaterRelayPins[id], LOW);
		}
	}
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

extern BoilerSettingStructure boilerSettings;

boolean circPumpStarting()
{
	return circPumpStartingAlarm < 0xFF;
}

void circPumpPumpOn()
{
	if (circPumpStarting() || _isBoilerRelayOn(BL_CIRC_PUMP))
		return;

	if (boilerSettings.Mode == BOILER_MODE_WINTER || boilerSettings.Mode == BOILER_MODE_WINTER_AWAY) // needs 5 min delay
	{
		circPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, circPumpOnTimer, "", 0);

		PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
	}
	else
		_boilerRelayOn(BL_CIRC_PUMP);
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
		_boilerRelayOff(BL_CIRC_PUMP);
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


boolean state_set_error_bit(int mask)
{
	if (!state_is_error_bit_set(mask))
	{
		thermostatControllerState |= mask;
		PublishControllerState();
		return true;
	}

	return false;
}

boolean state_clear_error_bit(int mask)
{
	if (state_is_error_bit_set(mask))
	{
		thermostatControllerState &= ~mask;
		PublishControllerState();
		return true;
	}

	return false;
}
