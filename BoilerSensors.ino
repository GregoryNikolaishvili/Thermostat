#define RREF 4000.0

Adafruit_MAX31865 solarSensor = Adafruit_MAX31865(PIN_MAX31865_SELECT);

Temperature* boilerSensorsValues[BOILER_SENSOR_COUNT];

BoilerSettingStructure boilerSettings;

void InitTemperatureSensors()
{
	for (int i = 0; i < BOILER_SENSOR_COUNT; i++)
		boilerSensorsValues[i] = new Temperature();

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
	if (boilerSettings.Mode != BOILER_MODE_WINTER)
	{
		for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
			heaterRelayOff(id);
		circPumpPumpOff();
		burnerOff();
		return;
	}

	ProcessRoomSensors();
}

int getBoilerT(byte id)
{
	return boilerSensorsValues[id]->getValue();
}

void setBoilerT(byte id, int value)
{
	int old = boilerSensorsValues[id]->getValue();
	boilerSensorsValues[id]->addValue(value);
	if (old != boilerSensorsValues[id]->getValue())
		PublishBoilerSensorT(id, false);
}

void CheckCirculatingPump()
{
	bool allHeaterRelaysAreOff = true;

	for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
	{
		if (isHeaterRelayOn(id))
		{
			allHeaterRelaysAreOff = false;
			break;
		}
	}

	if (allHeaterRelaysAreOff)
		circPumpPumpOff();
	else
		circPumpPumpOn();
}


Temperature::Temperature()
{
	clear();
}


void Temperature::clear()
{
	lastValue = T_UNDEFINED;
	idx = 0;
	cnt = 0;
	lastReadingTime = 0;
	for (byte i = 0; i < LINEAR_REGRESSION_POINT_COUNT; i++)
	{
		oldValues[i] = 0;
	}
}

void Temperature::addValue(const int value)
{
	lastValue = value;
	oldValues[idx++] = value;
	if (idx >= LINEAR_REGRESSION_POINT_COUNT)
		idx = 0;  // faster than %

	// update count as last otherwise if( _cnt == 0) above will fail
	if (cnt < LINEAR_REGRESSION_POINT_COUNT)
		cnt++;

	lastReadingTime = now();
}

int Temperature::getValue()
{
	return lastValue;
}

time_t Temperature::getLastReadingTime()
{
	return lastReadingTime;
}

char Temperature::getTrend()
{
	if (cnt < 2)
		return '=';

	// initialize variables
	float xbar = 0;
	float ybar = 0;
	float xybar = 0;
	float xsqbar = 0;

	// calculations required for linear regression
	for (byte j = 0; j < LINEAR_REGRESSION_POINT_COUNT; j++) {
		byte i = (j + idx) % LINEAR_REGRESSION_POINT_COUNT;

		//Serial.print("J = "); Serial.print(j);
		//Serial.print(", I = "); Serial.print(i);
		//Serial.print(", V = "); Serial.println(oldValues[i]);

		xbar = xbar + j;
		ybar = ybar + oldValues[i];
		xybar = xybar + j * oldValues[i];
		xsqbar = xsqbar + j * j;
	}

	xbar = xbar / LINEAR_REGRESSION_POINT_COUNT;
	ybar = ybar / LINEAR_REGRESSION_POINT_COUNT;
	xybar = xybar / LINEAR_REGRESSION_POINT_COUNT;
	xsqbar = xsqbar / LINEAR_REGRESSION_POINT_COUNT;

	// simple linear regression algorithm
	float slope = (xybar - xbar * ybar) / (xsqbar - xbar * xbar);
	//float intercept = ybar - slope * xbar;

	//Serial.print("xbar = "); Serial.println(xbar);
	//Serial.print("ybar = "); Serial.println(ybar);
	//Serial.print("xybar = "); Serial.println(xybar);
	//Serial.print("xsqbar = "); Serial.println(xsqbar);

	//Serial.print("slope = "); Serial.println(slope);
	//Serial.print("intercept = "); Serial.println(intercept);

	if (slope < 0)
		return '-';
	else
		if (slope > 0)
			return '+';
		else
			return '=';

}

// boiler & solar
void ProcessTemperatureSensors()
{
	int T1 = readSolarPaneT();
	int T2 = readTankBottomT();
	int T3 = readTankTopT();

	int TF = readFurnaceT();

	Serial.print("T1 = "); Serial.println(T1);
	Serial.print("T2 = "); Serial.println(T2);
	Serial.print("T3 = "); Serial.println(T3);
	Serial.print("T_furnace = "); Serial.println(TF);

	setBoilerT(T_SOLAR_PANEL_T, T1);
	setBoilerT(T_TANK_BOTTOM, T2);
	setBoilerT(T_TANK_TOP, T3);
	setBoilerT(T_FURNACE, TF);

	//if (boilerSettings.Mode == BOILER_MODE_SUMMER)

	if (!isValidT(T1))
	{
		if (state_set_error_bit(ERR_T1))
		{
			solarPumpOn();
			Serial.println(F("Solar sensor fail (T1)"));
		}
	}
	else
		if (state_clear_error_bit(ERR_T1))
			Serial.println(F("Solar sensor restored (T1)"));


	if (!isValidT(T3))
	{
		if (state_set_error_bit(ERR_T3))
			Serial.println(F("Boiler sensor fail (T3)"));
		T3 = T2;
	}
	else
		if (state_clear_error_bit(ERR_T3))
			Serial.println(F("Boiler sensor restored (T3)"));

	if (!isValidT(T2))
	{
		if (state_set_error_bit(ERR_T2))
			Serial.println(F("Boiler sensor fail (T2)"));

		if (isValidT(T3))
			T2 = T3;
		else
			burnerOff();
	}
	else
		if (state_clear_error_bit(ERR_T2))
			Serial.println(F("Boiler sensor restored (T2)"));


	if (!isValidT(TF))
	{
		if (state_set_error_bit(ERR_TF))
		{
			burnerOff();
			Serial.println(F("Furnace sensor fail (TF)"));
		}
	}
	else
		if (state_clear_error_bit(ERR_TF))
			Serial.println(F("Furnace sensor restored (TF)"));

	//////////////////

	if (isValidT(T1) && (T1 >= boilerSettings.EmergencyCollectorSwitchOffT))
	{
		solarPumpOff();

		if (state_set_error_bit(ERR_EMOF))
			Serial.println(F("EMOF activated"));
		return;
	}

	if (state_is_error_bit_set(ERR_EMOF) && isValidT(T1) && (T1 >= boilerSettings.EmergencyCollectorSwitchOnT))
	{
		Serial.println(F("EMOF_Activated and EmergencyCollectorSwitchOnT reached"));
		solarPumpOff();
		return;
	}

	if (state_clear_error_bit(ERR_EMOF))
		Serial.println(F("EMOF deactivated"));


	if (isValidT(T3) && (T3 >= 950)) // 95 degree is absolute max for boiler tank
	{
		solarPumpOff();
		burnerOff();

		if (state_set_error_bit(ERR_95_DEGREE))
			Serial.println(F("95 degree in tank activated"));
		return;
	}

	if (state_clear_error_bit(ERR_95_DEGREE))
		Serial.println(F("95 degree in tank deactivated"));

	if (isValidT(T1) && (T1 >= boilerSettings.CollectorCoolingT)) // CMX, 110
	{
		solarPumpOn();
		if (state_set_error_bit(ERR_CMX))
			Serial.println(F("CMX activated"));
		return;
	}

	if (state_clear_error_bit(ERR_CMX))
		Serial.println(F("CMX deactivated"));

	if (isValidT(T3) && (T3 >= boilerSettings.MaxTankT)) // SMX, 70
	{
		solarPumpOff();
		burnerOff();

		if (state_set_error_bit(ERR_SMX))
			Serial.println(F("SMX activated"));
		return;
	}

	if (state_clear_error_bit(ERR_SMX))
		Serial.println(F("SMX deactivated"));

	if (isValidT(T1) && isValidT(T2))
	{
		if (!isSolarPumpOn() && (T1 >= T2 + boilerSettings.CollectorSwitchOnTempDiff))
		{
			solarPumpOn();
		}
		else
			if (isSolarPumpOn() && (T1 < T2 + boilerSettings.CollectorSwitchOffTempDiff))
			{
				solarPumpOff();
			}
	}

	if (isValidT(T3) && (boilerSettings.Mode == BOILER_MODE_SUMMER_POOL))
	{
		if (T3 >= boilerSettings.PoolSwitchOnT)
			circPumpPumpOn();
		else
			if (T3 < boilerSettings.PoolSwitchOffT)
				circPumpPumpOff();
	}
}

int lastGoodSolarPanelTemperature = T_UNDEFINED;
int solarPanelTemperatureErrorCount = 0;

int readSolarPaneT()
{
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

		solarPanelTemperatureErrorCount++;
		if (solarPanelTemperatureErrorCount <= 5)
			return lastGoodSolarPanelTemperature;

		return T_UNDEFINED;
	}
	else
	{
		lastGoodSolarPanelTemperature = round(temperature) * 10;
		solarPanelTemperatureErrorCount = 0;
		return lastGoodSolarPanelTemperature;
	}
}

