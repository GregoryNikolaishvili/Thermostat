Adafruit_MAX31865 solarSensor = Adafruit_MAX31865(PIN_MAX31865_SELECT);

Temperature* boilerSensorsValues[BOILER_SENSOR_COUNT];

bool isBoilerTankOverheated = false;

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;

bool warning_SolarSensorFail_IsActivated = false;
bool warning_TankBottomSensorFail_IsActivated = false;
bool warning_TankTopSensorFail_IsActivated = false;
bool warning_HelioPressure_IsActivated = false;

static void initTemperatureSensors() {
  solarSensor.begin(MAX31865_2WIRE);
  solarSensor.enable50Hz(true);

  for (int i = 0; i < BOILER_SENSOR_COUNT; i++)
    boilerSensorsValues[i] = new Temperature(6);

  initDS18b20TempSensors();

  solarSensor.readRTD();
  startDS18B20TemperatureMeasurements();
}

static void ReInitBoiler() {
  if (boilerSettings.Mode != BOILER_MODE_WINTER) {
    for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
      heaterRelaySetValue(id, 100);  // 100% open
    circPumpPumpOff();
    burnerOff();
    return;
  }
}

static int getBoilerT(byte id) {
  return boilerSensorsValues[id]->getCurrentValue();
}

static int setBoilerT(byte id, int value) {
  Temperature* temperature = boilerSensorsValues[id];
  int old = temperature->getCurrentValue();
  int newValue = temperature->process(value);
  if (old != newValue)
    PublishBoilerSensorT(id);
  return newValue;
}

// boiler & solar
static void ProcessTemperatureSensors(bool& publishError) {
  int TSolar = readSolarPaneT();  // Solar panel T

  int TBottom = readTankBottomT();  // Tank bottom
  int TTop = readTankTopT();        // Tank top

  int TF = readFurnaceT();

  //	Serial.print("T1 = "); Serial.println(TSolar);
  //	Serial.print("T2 = "); Serial.println(T2);
  //	Serial.print("T3 = "); Serial.println(T3);
  //	Serial.print("T_furnace = "); Serial.println(TF);

  if (!isValidT(TSolar)) {
    if (!warning_SolarSensorFail_IsActivated) {
      publishError = true;
      warning_SolarSensorFail_IsActivated = true;
    }
  } else {
    if (warning_SolarSensorFail_IsActivated) {
      publishError = true;
      warning_SolarSensorFail_IsActivated = false;
    }
  }

  if (!isValidT(TBottom)) {
    if (!warning_TankBottomSensorFail_IsActivated) {
      publishError = true;
      warning_TankBottomSensorFail_IsActivated = true;
    }
  } else {
    if (warning_TankBottomSensorFail_IsActivated) {
      publishError = true;
      warning_TankBottomSensorFail_IsActivated = false;
    }
  }

  if (!isValidT(TTop)) {
    if (!warning_TankTopSensorFail_IsActivated) {
      publishError = true;
      warning_TankTopSensorFail_IsActivated = true;
    }
  } else {
    if (warning_TankTopSensorFail_IsActivated) {
      publishError = true;
      warning_TankTopSensorFail_IsActivated = false;
    }
  }

  TSolar = setBoilerT(T_SOLAR_PANEL_T, TSolar);
  TBottom = setBoilerT(T_TANK_BOTTOM, TBottom);
  TTop = setBoilerT(T_TANK_TOP, TTop);
  TF = setBoilerT(T_FURNACE, TF);

  // Read sensor data
  if (!isValidT(TSolar)) {
    solarPumpOn();
  }

  if (!isValidT(TTop)) {
    TTop = TBottom;
  }

  if (!isValidT(TBottom) && isValidT(TTop)) {
    TBottom = TTop;
  }

  if (!isValidT(TF)) {
    burnerOff();
  }

  int TBoiler = TTop;
  if (TBottom > TTop)
    TBoiler = TBottom;

  ////////////////// End of Read sensor data

  bool isSolarOk = CheckSolarPanels(TSolar, publishError);

  CheckBoilerTank(TBoiler, publishError);

  if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > boilerSettings.CollectorMinimumSwitchOnT)  // T2 = Tank bottom
  {
    if (!isSolarPumpOn() && (TSolar >= TBottom + boilerSettings.CollectorSwitchOnTempDiff)) {
      solarPumpOn();
    } else {
      if (isSolarPumpOn() && (TSolar < TBottom + boilerSettings.CollectorSwitchOffTempDiff)) {
        solarPumpOff();
      }
    }
  }

  if (!isBoilerTankOverheated && isValidT(TTop))  // T3 = Tank top
  {
    if (boilerSettings.Mode == BOILER_MODE_SUMMER_POOL) {
      if (!poolPumpIsOn) {
        circPumpPumpOff();
      } else {
        if (TTop >= boilerSettings.PoolSwitchOnT)
          circPumpPumpOn();
        else if (TTop < boilerSettings.PoolSwitchOffT)
          circPumpPumpOff();
      }
    }
  }
}

static bool CheckSolarPanels(int TSolar, bool& publishError) {
  if (isValidT(TSolar)) {
    // Is solar collector too hot?
    if (TSolar >= boilerSettings.CollectorEmergencySwitchOffT ||                             // 140
        (warning_EMOF_IsActivated && TSolar >= boilerSettings.CollectorEmergencySwitchOnT))  // 120
    {
      solarPumpOff();
      if (!warning_EMOF_IsActivated) {
        warning_EMOF_IsActivated = true;
        publishError = true;
        Serial.println(F("EMOF activated"));
      }
      return false;
    }

    if (warning_EMOF_IsActivated) {
      warning_EMOF_IsActivated = false;
      publishError = true;
      Serial.println(F("EMOF deactivated"));
    }

    // Is solar collector too cold?
    if (TSolar <= boilerSettings.CollectorAntifreezeT)  // 4
    {
      solarPumpOn();
      if (!warning_CFR_IsActivated) {
        warning_CFR_IsActivated = true;
        publishError = true;
        Serial.println(F("CFR activated"));
      }
      return false;
    }

    if (warning_CFR_IsActivated) {
      warning_CFR_IsActivated = false;
      publishError = true;
      Serial.println(F("CFR deactivated"));
    }
  }

  return true;
}

void CheckBoilerTank(int TBoiler, bool& publishError) {
  if (!isValidT(TBoiler)) {
    burnerOff();
    return;
  }

  bool turnBurnerOn = true;
  if (TBoiler >= boilerSettings.MaxTankT || (warning_SMX_IsActivated && TBoiler >= boilerSettings.MaxTankT - 50))  // SMX, 60
  {
    turnBurnerOn = false;
    burnerOff();

    if (!warning_SMX_IsActivated) {
      warning_SMX_IsActivated = true;
      publishError = true;
      Serial.println(F("SMX activated"));
    }
  } else {
    if (warning_SMX_IsActivated) {
      warning_SMX_IsActivated = false;
      publishError = true;
      Serial.println(F("SMX deactivated"));
    }
  }

  if (TBoiler >= boilerSettings.AbsoluteMaxTankT || (warning_MAXT_IsActivated && TBoiler >= boilerSettings.AbsoluteMaxTankT - 50))  // 95 degree is absolute max for boiler tank
  {
    isBoilerTankOverheated = true;
    turnBurnerOn = false;
    burnerOff();

    // Try to cool down boiler with all means
    for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
      heaterRelaySetValue(id, 100);  // 100% open

    circPumpPumpOn();  // Turn on recirculating pump

    if (!warning_MAXT_IsActivated) {
      warning_MAXT_IsActivated = true;
      publishError = true;
      Serial.println(F("95 degree in tank activated"));
    }
    return;
  }

  isBoilerTankOverheated = false;
  if (warning_MAXT_IsActivated) {
    warning_MAXT_IsActivated = false;
    publishError = true;
    Serial.println(F("95 degree in tank deactivated"));
  }
  if (turnBurnerOn)
    burnerOn();
}

// Returns temperature multiplied by 10, or T_UNDEFINED
int readSolarPaneT() {
  //const float RREF = 4300.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
  //const float RREF = 4265.0;// https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code
  const float RREF = 4000.0;  // https://learn.adafruit.com/adafruit-max31865-rtd-pt100-amplifier/arduino-code

  float Rt = solarSensor.readRTD();
  Rt /= 32768;
  Rt *= RREF;

  //Serial.println(Rt);

  float temperature = solarSensor.temperature(1000.0, RREF);

  //Serial.println(temperature);

  //Serial.print("RTD value: "); Serial.println(lastReadSolarPanelRTD);
  //Serial.print(F("RTD Resistance = ")); Serial.println(RREF * lastReadSolarPanelRTD / 32768, 8);
  //Serial.print(F("RTD Temperature = ")); Serial.println(temperature);

  // Check and print any faults
  uint8_t fault = solarSensor.readFault();
  if (fault) {
    solarSensor.clearFault();

    Serial.print(F("Fault 0x"));
    Serial.println(fault, HEX);
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

    return T_UNDEFINED;
  }

  return round(temperature) * 10;  // 1 degree precision is enough
}

//bool checkAllHeaterRelaysAreOff()
//{
//	bool allHeaterRelaysAreOff = true;
//
//	for (byte i = 0; i < USED_HEATER_RELAY_COUNT; i++)
//	{
//		if (heaterRelayGetValue(i) > 0)
//		{
//			allHeaterRelaysAreOff = false;
//			break;
//		}
//	}
//
//	return allHeaterRelaysAreOff;
//}