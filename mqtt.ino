#include "utility/w5100.h"

byte mac[] = { 0x54, 0x34, 0x41, 0x30, 0x30, 0x08 };
IPAddress ip(192, 168, 1, 8);

EthernetClient ethClient;
PubSubClient mqttClient("192.168.1.23", 1883, callback, ethClient);     // Initialize a MQTT mqttClient instance

#define MQTT_BUFFER_SIZE 256

char buffer[MQTT_BUFFER_SIZE];

bool doLog = true;

void InitEthernet()
{
	Serial.println(F("Starting ethernet.."));

	Ethernet.begin(mac, ip);
	ethClient.setConnectionTimeout(2000);

	W5100.setRetransmissionTime(0x07D0);
	W5100.setRetransmissionCount(3);

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

//void PublishMqtt(char* topic, char* message, boolean retained)
//{
//	int len = strlen(message);
//
//	Serial.print(F("Publish. topic="));
//	Serial.print(topic);
//	Serial.print(F(", length="));
//	Serial.print(len);
//
//	Serial.print(F(", payload="));
//	Serial.println(message);
//
//	mqttClient.publish(topic, (byte*)message, len, retained);
//}

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
		if (mqttClient.connect("boiler", "hub/controller/boiler", 1, true, "{\"state\":\"disconnected\"}")) {

			Serial.println(F("connected"));

			// Once connected, publish an announcement...
			mqttClient.publish("hub/controller/boiler", "{\"state\":\"connected\"}", true);  // Publish ethernet connected status to MQTT topic

			// ... and resubscribe
			mqttClient.subscribe("chac/ts/#", 1);     // Subscribe to a MQTT topic, qos = 1

			mqttClient.publish("hubcommand/gettime2", "chac/ts/settime2", false); // request time

			PublishBoilerSettings();
			PublishRoomSensorSettings();
			PublishAllStates(true);
		}
		else {
			Serial.print(F("failed, rc="));
			Serial.println(mqttClient.state());
		}
	}
}

void PublishAllStates(bool isInitialState) {
	if (!mqttClient.connected()) return;

	doLog = false;

	if (!isInitialState)
		for (byte id = 0; id < BOILER_SENSOR_COUNT; id++)
			PublishBoilerSensorT(id);

	for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
		PublishHeaterRelayState(id, heaterRelayGetValue(id));

	for (byte id = 0; id < BOILER_RELAY_COUNT; id++)
		PublishBoilerRelayState(id, _isBoilerRelayOn(id));

	doLog = true;
}

//void PublishRoomSensorT(byte idx)
//{
//	if (!mqttClient.connected()) return;
//
//	int id = roomSensorsIds[idx];
//	int T = getRoomT(id);
//
//	char* topic = "cha/ROOM_SENSOR/????";
//	setHexInt16(topic, id, 16);
//
//	setHexT(buffer, T, 0);
//	setHexInt16(buffer, now() - roomSensorsLastTime[idx], 4);
//
//	PublishMqtt(topic, buffer, 8, false);
//}

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
	//buffer[4] = bsv->getTrend();
	////setHexInt16(buffer, now() - bsv->getLastReadingTime(), 5);
	//PublishMqtt(topic, buffer, 5, true);
	PublishMqtt(topic, buffer, 4, true);
}


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

	idx = setHexInt16(buffer, warning_EMOF_IsActivated | 2 * warning_CFR_IsActivated | 4 * warning_SMX_IsActivated | 8 * warning_SMX_IsActivated | 16 * warning_MAXT_IsActivated, idx);

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

void PublishRoomSensorSettings()
{
	if (!mqttClient.connected()) return;

	const char* topic = "cha/ts/settings/rs";

	int idx = 0;
	idx = setHexByte(buffer, roomSensorSettingsCount, idx); // count 2 bytes
	for (byte i = 0; i < roomSensorSettingsCount; i++)
	{
		idx = setHexInt16(buffer, roomSensorSettings[i].id, idx); // id 4 bytes
		idx = setHexT(buffer, roomSensorSettings[i].targetT, idx); // targetT 4 bytes
		idx = setHexByte(buffer, roomSensorSettings[i].relayId, idx); // relayId 2 bytes
	}

	PublishMqtt(topic, buffer, idx, true);
}

//void PublishRoomSensorNamesAndOrder()
//{
//	if (!mqttClient.connected()) return;
//
//	const char* topic = "cha/ts/names/rs";
//
//	int length = eeprom_read_word((uint16_t *)STORAGE_ADDRESS_DATA);
//	//Serial.print("load name & order data. len=");
//	//Serial.println(length);
//
//	for (int i = 0; i < length; i++)
//	{
//		byte b = eeprom_read_byte((const uint8_t *)(STORAGE_ADDRESS_DATA + 2 + i));
//		buffer[i] = b;
//	}
//
//	PublishMqtt(topic, buffer, length, true);
//}

//void PublishAlert(char*message)
//{
//	PublishMqtt("cha/alert", message, strlen(message), false);
//	Serial.print("Alert: ");
//	if (message != NULL)
//		Serial.println(message);
//
//}

void callback(char* topic, byte * payload, unsigned int len) {


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
		PublishAllStates(false);
		return;
	}

	if (strcmp(topic, "chac/ts/diag") == 0)
	{
		publishTempSensorData();
		return;
	}

	if (len == 0)
		return;

	// Data arrived from room sensors (acurite) via AcuHack 
	if (strncmp(topic, "chac/ts/state/rs/", 17) == 0)
	{
		int id = readHex(topic + 17, 4);

		strncpy(buffer, (char*)payload, len);
		buffer[len] = 0;
		int value = atoi(buffer); // T in decimal

		//Serial.print(F("Room sensor data: Id="));
		//Serial.print(id);
		//Serial.print(F(", T="));
		//Serial.println(value);

		addRoomT(id, value);

		ProcessRoomSensor(id, true);
		return;
	}

	if (strncmp(topic, "chac/ts/state/bl/", 17) == 0)
	{
		byte id = hexCharToByte(topic[17]);
		bool value = payload[0] != '0';

		_boilerRelaySet(id, value);
		return;
	}

	if (strncmp(topic, "chac/ts/state/hr/", 17) == 0)
	{
		byte id = hexCharToByte(topic[16]);
		bool value = payload[0] != '0';

		heaterRelaySetValue(id, value ? 100 : 0);
		return;
	}

	// Set settings of room sensors
	if (strncmp(topic, "chac/ts/settings2/rs/", 21) == 0)
	{
		char* p = (char*)topic;
		p += 21;
		int id = readHex(p, 4);

		p = (char*)payload;

		Serial.print(F("New room sensor settings. Id="));
		Serial.println(id);

		byte roomSensorIdx = -1;
		for (byte i = 0; i < roomSensorSettingsCount; i++)
		{
			if (roomSensorSettings[i].id == id)
			{
				roomSensorIdx = i;
				break;
			}
		}

		// setting not found
		if (roomSensorIdx < 0 && roomSensorSettingsCount < MAX_ROOM_SENSORS)
		{
			roomSensorIdx = roomSensorSettingsCount++;
			roomSensorSettings[roomSensorIdx].relayId = 0; // Inactive
		}

		if (roomSensorIdx >= 0)
		{
			roomSensorSettings[roomSensorIdx].id = id;
			roomSensorSettings[roomSensorIdx].targetT = readHexT(p);
			if (len > 4)
			{
				p += 4;
				roomSensorSettings[roomSensorIdx].relayId = readHex(p, 2);
			}
			saveRoomSensorSettings(true);
			ProcessRoomSensors();
		}
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
