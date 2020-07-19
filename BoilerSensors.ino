#define RREF 4000.0

Adafruit_MAX31865 solarSensor = Adafruit_MAX31865(PIN_MAX31865_SELECT);

Temperature* boilerSensorsValues[BOILER_SENSOR_COUNT];

BoilerSettingStructure boilerSettings;

bool isBoilerTankOverheated = false;

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;


void InitTemperatureSensors()
{
	for (int i = 0; i < BOILER_SENSOR_COUNT; i++)
		boilerSensorsValues[i] = new Temperature(10);

	initDS18b20TempSensors();

	solarSensor.begin(MAX31865_2WIRE);  // set to 2WIRE or 4WIRE as necessary
	delay(100);
	solarSensor.readRTD_step1();
	delay(10);
	solarSensor.readRTD_step2();
	delay(65);
	lastReadSolarPanelRTD = solarSensor.readRTD_step3();

	startDS18B20TemperatureMeasurements();
}

void ReInitBoiler()
{
	if (boilerSettings.Mode != BOILER_MODE_WINTER && boilerSettings.Mode != BOILER_MODE_WINTER_AWAY)
	{
		for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
			heaterRelaySetValue(id, 100); // 100% open
		circPumpPumpOff();
		burnerOff();
		return;
	}

	ProcessRoomSensors();
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

void CheckCirculatingPump()
{
	if (checkAllHeaterRelaysAreOff())
		circPumpPumpOff();
	else
		circPumpPumpOn();
}



// boiler & solar
void ProcessTemperatureSensors()
{
	int TSolar = readSolarPaneT(); // Solar panel T
	int T2 = readTankBottomT(); // Tank bottom
	int T3 = readTankTopT(); // Tank top

	int TF = readFurnaceT();

//	Serial.print("T1 = "); Serial.println(TSolar);
//	Serial.print("T2 = "); Serial.println(T2);
//	Serial.print("T3 = "); Serial.println(T3);
//	Serial.print("T_furnace = "); Serial.println(TF);

	TSolar = setBoilerT(T_SOLAR_PANEL_T, TSolar);
	T2 = setBoilerT(T_TANK_BOTTOM, T2);
	T3 = setBoilerT(T_TANK_TOP, T3);
	TF = setBoilerT(T_FURNACE, TF);

	// Read sensor data
	if (!isValidT(TSolar))
	{
		solarPumpOn();
		//if (state_set_error_bit(ERR_T1))
		//{
		//	solarPumpOn();
		//	Serial.println(F("Solar sensor fail (T1)"));
		//}
	}
	//else
	//{
	//	if (state_clear_error_bit(ERR_T1))
	//		Serial.println(F("Solar sensor restored (T1)"));
	//}

	if (!isValidT(T3))
	{
		//if (state_set_error_bit(ERR_T3))
		//	Serial.println(F("Boiler sensor fail (T3)"));
		T3 = T2;
	}
	//else
	//{
	//	if (state_clear_error_bit(ERR_T3))
	//		Serial.println(F("Boiler sensor restored (T3)"));
	//}

	if (!isValidT(T2))
	{
		//if (state_set_error_bit(ERR_T2))
		//	Serial.println(F("Boiler sensor fail (T2)"));
		if (isValidT(T3))
			T2 = T3;
	}
	//else
	//{
	//	if (state_clear_error_bit(ERR_T2))
	//		Serial.println(F("Boiler sensor restored (T2)"));
	//}

	if (!isValidT(TF))
	{
		burnerOff();
		//if (state_set_error_bit(ERR_TF))
		//{
		//	burnerOff();
		//	Serial.println(F("Furnace sensor fail (TF)"));
		//}
	}
	//else
	//{
	//	if (state_clear_error_bit(ERR_TF))
	//		Serial.println(F("Furnace sensor restored (TF)"));
	//}

	int TBoiler = T3;
	if (T2 > T3)
		TBoiler = T2;

	////////////////// End of Read sensor data

	bool isSolarOk = CheckSolarPanels(TSolar);

	CheckBoilerTank(TBoiler);

	if (isSolarOk && isValidT(TSolar) && isValidT(T2) && TSolar > boilerSettings.CollectorMinimumSwitchOnT) // T2 = Tank bottom
	{
		if (!isSolarPumpOn() && (TSolar >= T2 + boilerSettings.CollectorSwitchOnTempDiff))
		{
			solarPumpOn();
		}
		else
		{
			if (isSolarPumpOn() && (TSolar < T2 + boilerSettings.CollectorSwitchOffTempDiff))
			{
				solarPumpOff();
			}
		}
	}

	if (!isBoilerTankOverheated && isValidT(T3)) // T3 = Tank top
	{
		if (boilerSettings.Mode == BOILER_MODE_SUMMER_POOL || boilerSettings.Mode == BOILER_MODE_SUMMER_POOL_AWAY)
		{
			if (T3 >= boilerSettings.PoolSwitchOnT)
				circPumpPumpOn();
			else if (T3 < boilerSettings.PoolSwitchOffT)
				circPumpPumpOff();
		}
	}
}

bool CheckSolarPanels(int TSolar)
{
	bool publish = false;

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
				publish = true;
				Serial.println(F("EMOF activated"));
			}
			return false;
		}
		else
		{
			if (warning_EMOF_IsActivated)
			{
				warning_EMOF_IsActivated = false;
				publish = true;
				Serial.println(F("EMOF deactivated"));
			}
		}

		// Is solar collector too cold?
		if (TSolar <= boilerSettings.CollectorAntifreezeT) // 4
		{
			solarPumpOn();
			if (!warning_CFR_IsActivated)
			{
				warning_CFR_IsActivated = true;
				publish = true;
				Serial.println(F("CFR activated"));
			}
			return false;
		}

		if (warning_CFR_IsActivated)
		{
			warning_CFR_IsActivated = false;
			publish = true;
			Serial.println(F("CFR deactivated"));
		}
	}

	if (publish)
		PublishBoilerSettings();

	return true;
}

void CheckBoilerTank(int TBoiler)
{
	if (!isValidT(TBoiler))
	{
		burnerOff();
		return;
	}

	bool publish = false;

	bool turnBurnerOn = true;
	if (TBoiler >= boilerSettings.MaxTankT ||
		(warning_SMX_IsActivated && TBoiler >= boilerSettings.MaxTankT - 50)) // SMX, 60
	{
		turnBurnerOn = false;
		burnerOff();

		if (!warning_SMX_IsActivated)
		{
			warning_SMX_IsActivated = true;
			publish = true;
			Serial.println(F("SMX activated"));
		}
	}
	else
	{
		if (warning_SMX_IsActivated)
		{
			warning_SMX_IsActivated = false;
			publish = true;
			Serial.println(F("SMX deactivated"));
		}
	}

	if (TBoiler >= boilerSettings.AbsoluteMaxTankT
		|| (warning_MAXT_IsActivated && TBoiler >= boilerSettings.AbsoluteMaxTankT - 50)) // 95 degree is absolute max for boiler tank
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
			publish = true;
			Serial.println(F("95 degree in tank activated"));
		}
		return;
	}

	isBoilerTankOverheated = false;
	if (warning_MAXT_IsActivated)
	{
		warning_MAXT_IsActivated = false;
		publish = true;
		Serial.println(F("95 degree in tank deactivated"));
	}
	if (turnBurnerOn)
		burnerOn();
	
	if (publish)
		PublishBoilerSettings();
}

// Returns temperature multiplied by 10, or T_UNDEFINED
int readSolarPaneT()
{
	int T;
	float temperature = solarSensor.temperature(lastReadSolarPanelRTD, 1000.0, RREF);

	//Serial.print("RTD value: "); Serial.println(lastReadSolarPanelRTD);
	//Serial.print(F("RTD Resistance = ")); Serial.println(RREF * lastReadSolarPanelRTD / 32768, 8);
	//Serial.print(F("RTD Temperature = ")); Serial.println(temperature);

	// Check and print any faults
	uint8_t fault = solarSensor.readFault();
	if (fault) {
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
		solarSensor.clearFault();

		T = T_UNDEFINED;
	}
	else
	{
		T = round(temperature) * 10; // 1 degree precision is enough
	}

	return T;
}
