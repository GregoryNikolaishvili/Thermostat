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
		byte id = hexCharToByte(topic[17]);
		bool value = payload[0] != '0';

		_boilerRelaySet(id, value);
		return;
	}

	if (strncmp(topic, "chac/ts/state/hr/", 17) == 0) // Heating
	{
		byte id = hexCharToByte(topic[17]);
		bool value = payload[0] != '0';

		if (!isBoilerTankOverheated)
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
