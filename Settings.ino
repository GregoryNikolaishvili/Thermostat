int sunriseMin;
int sunsetMin;

const byte DEF_SETTINGS_VERSION_BOILER = 0x0D;
const byte DEF_SETTINGS_VERSION_ROOM_SENSORS = 0x0B;

const char OFF_SUNRISE = 'S';
const char OFF_TIME = 'T';
const char OFF_DURATION = 'D';


void readSettings()
{
	boilerSettings.Mode = 'N';
	boilerSettings.CollectorSwitchOnTempDiff = 80; // 2...20
	boilerSettings.CollectorSwitchOffTempDiff = 40; // 0 ... EMOF - 2

	boilerSettings.CollectorEmergencySwitchOffT = 1400; // EMOF, 110...200
	boilerSettings.CollectorEmergencySwitchOnT = 1200; // EMON, 0...EMOF - 3

	boilerSettings.CollectorMinimumSwitchOnT = 100; // CMN, -10...90
	boilerSettings.CollectorAntifreezeT = 40; // CFR, -10...10
	boilerSettings.MaxTankT = 600; // SMX, Max 95
	boilerSettings.AbsoluteMaxTankT = 950; // 95

	boilerSettings.PoolSwitchOnT = 550;
	boilerSettings.PoolSwitchOffT = 500;

	boilerSettings.BackupHeatingTS1_Start = 400; // 4am
	boilerSettings.BackupHeatingTS1_End = 500; // 5am
	boilerSettings.BackupHeatingTS1_SwitchOnT = 400; // 10 - OFF -> 2
	boilerSettings.BackupHeatingTS1_SwitchOffT = 450; // ON + 2 -> 80

	boilerSettings.BackupHeatingTS2_Start = 800; // 8am
	boilerSettings.BackupHeatingTS2_End = 1200; // 2pm
	boilerSettings.BackupHeatingTS2_SwitchOnT = 500;
	boilerSettings.BackupHeatingTS2_SwitchOffT = 550;

	boilerSettings.BackupHeatingTS3_Start = 1700; // 5pm
	boilerSettings.BackupHeatingTS3_End = 2300; // 11pm
	boilerSettings.BackupHeatingTS3_SwitchOnT = 500;
	boilerSettings.BackupHeatingTS3_SwitchOffT = 550;

	for (byte i = 0; i < MAX_ROOM_SENSORS; i++)
	{
		roomSensorSettings[i].id = 0;
		roomSensorSettings[i].targetT = 25;
		roomSensorSettings[i].relayId = 0;
	}

	// boiler
	byte v = eeprom_read_byte((uint8_t *)STORAGE_ADDRESS_BOILER_SETTINGS);

	if (v != DEF_SETTINGS_VERSION_BOILER)
	{
		eeprom_update_byte((uint8_t *)STORAGE_ADDRESS_BOILER_SETTINGS, DEF_SETTINGS_VERSION_BOILER);
		saveBoilerSettings(false);
	}
	else
	{
		eeprom_read_block((void*)&boilerSettings, (void*)(STORAGE_ADDRESS_BOILER_SETTINGS + 1), sizeof(boilerSettings));
		boilerSettingsChanged(false);
	}

	// room sensors
	v = eeprom_read_byte((uint8_t *)STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS);
	if (v != DEF_SETTINGS_VERSION_ROOM_SENSORS)
	{
		eeprom_update_byte((uint8_t *)STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS, DEF_SETTINGS_VERSION_ROOM_SENSORS);
		saveRoomSensorSettings(false);

		saveData("0000", 4);
	}
	else
	{
		roomSensorSettingsCount = eeprom_read_byte((uint8_t *)(STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS + 1));

		eeprom_read_block((void*)&roomSensorSettings, (void*)(STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS + 2), sizeof(roomSensorSettings));

		roomSensorSettingsChanged(false);
	}
}

void saveBoilerSettings(bool publish)
{
	eeprom_update_block((const void*)&boilerSettings, (void*)(STORAGE_ADDRESS_BOILER_SETTINGS + 1), sizeof(boilerSettings));

	boilerSettingsChanged(publish);
}

void saveRoomSensorSettings(bool publish)
{
	eeprom_update_byte((uint8_t *)(STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS + 1), roomSensorSettingsCount);
	eeprom_update_block((const void*)&roomSensorSettings, (void*)(STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS + 2), sizeof(roomSensorSettings));

	roomSensorSettingsChanged(publish);
}


void boilerSettingsChanged(bool publish)
{
	if (publish)
		PublishBoilerSettings();
}

void roomSensorSettingsChanged(bool publish)
{
	if (publish)
		PublishRoomSensorSettings();
}

void saveData(const void* data, int length)
{
	if (length > 256)
		length = 256;
	eeprom_update_word((uint16_t *)STORAGE_ADDRESS_DATA, length);
	eeprom_update_block(data, (void*)(STORAGE_ADDRESS_DATA + 2), length);
}


void resetAlarms(int tag, int tag2)
{
	freeAlarms();

	Alarm.alarmRepeat(0, resetAlarms, NULL, 0); // 00:00:00 at midnight. eventName = NULL means don't show in next event

	//OnOffSettingStructure* onOff;
	//for (byte id = 0; id < RELAY_COUNT; id++)
	//{
	//  onOff = &onOffSettings[id];
	//  if (onOff->isActive)
	//  {
	//    time_t tm_on = getOnTime(onOff);
	//    time_t tm_off = getOffTime(onOff);

	//    Alarm.alarmRepeat(tm_on, relayOnCheckMode, "Relay ON", id);
	//    Alarm.alarmRepeat(tm_off, relayOffCheckMode, "Relay OFF", id);

	//    Serial.print("Relay #");
	//    Serial.print(id);
	//    Serial.print(". ON: ");
	//    printTime(&Serial, tm_on);

	//    Serial.print(", OFF: ");
	//    printTime(&Serial, tm_off);
	//    Serial.println();
	//  }
	//}

	//  if (automaticMode)
	//    setCurrentRelayStates();
}

void freeAlarms()
{
	for (byte id = 0; id < dtNBR_ALARMS; id++)
	{
		if (Alarm.isAllocated(id))
			Alarm.free(id);
	}
}

void showNextEvent()
{
	AlarmClass* alarm = Alarm.getNextTriggerAlarm();
	if (alarm)
	{
		Serial.print(F("Next alarm event at: "));
		printTime(&Serial, alarm->value);
		Serial.print(F(", "));
		Serial.println(alarm->eventName);
	}
}


//time_t getOnTime(OnOffSettingStructure* onOff)
//{
//  return (sunsetMin + onOff->onOffset) * SECS_PER_MIN;
//}
//
//
//time_t getOffTime(OnOffSettingStructure* onOff)
//{
//  time_t value = 0;
//  switch (onOff->offType)
//  {
//    case OFF_SUNRISE:
//      value = (sunriseMin + onOff->offValue) * SECS_PER_MIN;
//      break;
//    case OFF_TIME:
//      value = onOff->offValue * SECS_PER_MIN;
//      break;
//    case OFF_DURATION:
//      value = getOnTime(onOff) + onOff->offValue * SECS_PER_MIN;
//      break;
//  }
//  return value;
//}
//
//void setCurrentRelayStates()
//{
//  OnOffSettingStructure* onOff;
//  for (byte id = 0; id < RELAY_COUNT; id++)
//  {
//    onOff = &onOffSettings[id];
//    if (onOff->isActive)
//    {
//      time_t tm_on = getOnTime(onOff);
//      time_t tm_off = getOffTime(onOff);
//
//      relaySet(id, getRelayStateByTime(tm_on / SECS_PER_MIN, tm_off / SECS_PER_MIN));
//    }
//    else
//      relaySet(id, false);
//  }
//}
//
//boolean getRelayStateByTime(int onTime, int offTime)
//{
//  time_t time_now = now();
//  time_now = time_now - previousMidnight(time_now);
//  int itime_now = time_now / SECS_PER_MIN;
//
//  boolean b;
//  if (onTime < offTime)
//  {
//    b = (itime_now >= onTime) && (itime_now < offTime);
//  }
//  else if (onTime > offTime)
//  {
//    int t = onTime;
//    onTime = offTime;
//    offTime = t;
//    b = !((itime_now >= onTime) && (itime_now < offTime));
//  }
//  else // if equals
//  {
//    b = false;
//  }
//  return b;
//}
