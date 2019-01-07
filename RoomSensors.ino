int roomSensorsIds[MAX_ROOM_SENSORS];
int roomSensorsValues[MAX_ROOM_SENSORS];
time_t roomSensorsLastTime[MAX_ROOM_SENSORS];

RoomSensorSettingStructure roomSensorSettings[MAX_ROOM_SENSORS];

byte roomSensorSettingsCount = 0;
byte roomSensorCount = 0;

// room sensors
void addRoomT(int id, int value)
{
	for (byte i = 0; i < roomSensorCount; i++)
	{
		if (roomSensorsIds[i] == id)
		{
			roomSensorsValues[i] = value;
			roomSensorsLastTime[i] = now();
			return;
		}
	}

	// sensor not found
	if (roomSensorCount < MAX_ROOM_SENSORS)
	{
		roomSensorsIds[roomSensorCount] = id;
		roomSensorsValues[roomSensorCount] = value;
		roomSensorsLastTime[roomSensorCount] = now();
		roomSensorCount++;

		Serial.print(F("Room sensor added: Id = "));
		Serial.print(id);
		Serial.print(F(", Count = "));
		Serial.println(roomSensorCount);
	}

	for (byte i = 0; i < roomSensorSettingsCount; i++)
	{
		if (roomSensorSettings[i].id == id)
			return;
	}

	// setting not found
	if (roomSensorSettingsCount < MAX_ROOM_SENSORS)
	{
		roomSensorSettings[roomSensorSettingsCount].id = id;
		roomSensorSettings[roomSensorSettingsCount].targetT = 220;
		roomSensorSettings[roomSensorSettingsCount].relayId = 0;
		roomSensorSettingsCount++;

		roomSensorSettingsChanged(true);
	}
}

int getRoomT(int id)
{
	for (byte i = 0; i < roomSensorCount; i++)
	{
		if (roomSensorsIds[i] == id)
			return roomSensorsValues[i];
	}

	// not found
	return T_UNDEFINED;
}

RoomSensorSettingStructure* getRoomSensorSettings(int id)
{
	for (byte i = 0; i < roomSensorSettingsCount; i++)
	{
		if (roomSensorSettings[i].id == id)
			return &roomSensorSettings[i];
	}
	return NULL;
}


void ProcessRoomSensors()
{
	if (boilerSettings.Mode != BOILER_MODE_WINTER)
		return;
	if (isBoilerTankOverheated)
		return;

	for (byte i = 0; i < roomSensorCount; i++)
	{
		int id = roomSensorsIds[i];

		ProcessRoomSensor(id, false);
	}

	CheckCirculatingPump();
}

void ProcessRoomSensor(int id, bool checkCirculatingPump)
{
	if (boilerSettings.Mode != BOILER_MODE_WINTER)
		return;
	if (isBoilerTankOverheated)
		return;

	int T = getRoomT(id);
	if (isValidT(T))
	{
		RoomSensorSettingStructure* rss = getRoomSensorSettings(id);

		if ((rss != NULL) && (rss->relayId > 0))
		{
			int delta = T - rss->targetT;
			bool relayIsOn = isHeaterRelayOn(rss->relayId);

			if (relayIsOn && (delta >= 2)) // 0.2 degrees
			{
				heaterRelayOff(rss->relayId);
			}
			else
				if (!relayIsOn && (delta <= -2)) // -0.2 degrees
				{
					heaterRelayOn(rss->relayId);
				}
		}
	}

	if (checkCirculatingPump)
		CheckCirculatingPump();
}
