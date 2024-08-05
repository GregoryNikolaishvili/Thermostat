#include "main.h"

#include <Adafruit_MAX31865.h>
#include <SolarTemperatureSensor.h>
#include <ArduinoJson.h>
#include <TimeAlarms.h>
#include <HASwitchX.h>

// Temperature* boilerSensorsValues[BOILER_SENSOR_COUNT];

const int TEMP_SENSOR_BOILER_BOTTOM = 0;
const int TEMP_SENSOR_BOILER_TOP = 1;
const int TEMP_SENSOR_FURNACE = 2;
// const int TEMP_SENSOR_RESERVE_1 = 3;

AlarmID_t heatingCirculationPumpStartingAlarm = 0xFF;

bool isBoilerTankOverheated = false;
bool poolPumpIsOn;

JsonDocument jsonDoc;

extern BoilerSettingStructure boilerSettings;

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;

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
    PIN_HR_16};

// static void ReInitBoiler() {
//   if (boilerSettings.Mode != BOILER_MODE_WINTER) {
//     for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
//       heaterRelaySetValue(id, 100);  // 100% open
//     heatingCirculationPumpPumpOff();
//     burnerOff();
//     return;
//   }
// }

SolarTemperatureReader solarT(PIN_MAX31865_SELECT);
extern TemperatureDS18B20 boilerT;

extern HASensorNumber solarTemperatureSensor;
extern HASensorNumber boilerTopTemperatureSensor;
extern HASensorNumber boilerBottomTemperatureSensor;
extern HASensor errorStatus;

extern HASwitchX heatingRecirculatingPump;
extern HASwitchX solarRecirculatingPump;

static void heatingCirculationPumpOnTimer();

static void solarPumpOn()
{
  digitalWrite(PIN_BL_SOLAR_PUMP, LOW);
  solarRecirculatingPump.setState(true);
}

void solarPumpOff()
{
  digitalWrite(PIN_BL_SOLAR_PUMP, HIGH);
  solarRecirculatingPump.setState(false);
}

bool isSolarPumpOn()
{
  return digitalRead(PIN_BL_SOLAR_PUMP) == LOW;
}

boolean isHeatingCirculationPumpStarting()
{
  return heatingCirculationPumpStartingAlarm < 0xFF;
}

static void heatingCirculationPumpOn()
{
  if (isHeatingCirculationPumpStarting() || digitalRead(PIN_BL_CIRC_PUMP) == LOW)
    return;

  if (boilerSettings.Mode == BOILER_MODE_WINTER) // needs 5 min delay
  {
    heatingCirculationPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, heatingCirculationPumpOnTimer);

    // PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
  }
  else
  {
    digitalWrite(PIN_BL_CIRC_PUMP, LOW);
    heatingRecirculatingPump.setState(true);
  }
}

static void heatingCirculationPumpPumpOff()
{
  if (heatingCirculationPumpStartingAlarm < 0xFF)
  {
    Alarm.free(heatingCirculationPumpStartingAlarm);
    heatingCirculationPumpStartingAlarm = 0xFF;
  }

  digitalWrite(PIN_BL_CIRC_PUMP, HIGH);
  heatingRecirculatingPump.setState(false);
}

static void heatingCirculationPumpOnTimer()
{
  if (heatingCirculationPumpStartingAlarm < 0xFF)
  {
    Alarm.free(heatingCirculationPumpStartingAlarm);
    heatingCirculationPumpStartingAlarm = 0xFF;
  }

  heatingCirculationPumpOn();
}

static void heaterRelayOn(byte id)
{
  if (id < HEATER_RELAY_COUNT)
  {
    digitalWrite(heaterRelayPins[id], LOW);
  }
}

static bool CheckSolarPanels(int TSolar)
{
  if (!isValidT(TSolar))
    return false;

  // Is solar collector too hot?
  if (
      (TSolar >= boilerSettings.CollectorEmergencySwitchOffT) // 140
      ||
      (warning_EMOF_IsActivated && TSolar >= boilerSettings.CollectorEmergencySwitchOnT))
  {
    solarPumpOff();

    if (!warning_EMOF_IsActivated)
      Serial.println(F("EMOF activated"));
    warning_EMOF_IsActivated = true;

    jsonDoc["EMOF"] = "Maximum collector temperature protection active";
    return false;
  }

  if (warning_EMOF_IsActivated)
  {
    warning_EMOF_IsActivated = false;

    jsonDoc.remove("EMOF");
    Serial.println(F("EMOF deactivated"));
  }

  // Is solar collector too cold?
  if (TSolar <= boilerSettings.CollectorAntifreezeT) // 4
  {
    solarPumpOn();

    if (!warning_CFR_IsActivated)
    {
      warning_CFR_IsActivated = true;
      Serial.println(F("CFR activated"));
    }

    jsonDoc["CFR"] = "Frost protection of collector active";
    return false;
  }

  if (warning_CFR_IsActivated)
  {
    warning_CFR_IsActivated = false;

    jsonDoc.remove("CFR");
    Serial.println(F("CFR deactivated"));
  }

  return true;
}

bool CheckBoilerTank(int TBoiler)
{
  if (!isValidT(TBoiler))
    return false;

  // Is tank super hot?
  if (
      (TBoiler >= boilerSettings.AbsoluteMaxTankT) // 95 degree is absolute max for boiler tank
      ||
      (warning_MAXT_IsActivated && TBoiler >= boilerSettings.AbsoluteMaxTankT - 5))
  {
    // Try to cool down boiler with all means
    for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
      heaterRelayOn(id);        // 100% open
    heatingCirculationPumpOn(); // Turn on recirculating pump

    if (!warning_MAXT_IsActivated)
      Serial.println(F("ABSMAXT activated"));
    warning_MAXT_IsActivated = true;

    jsonDoc["ABSMAXT"] = "Absolute maximum tank temperature (95) protection active";
    return false;
  }

  if (warning_MAXT_IsActivated)
  {
    warning_MAXT_IsActivated = false;

    jsonDoc.remove("ABSMAXT");
    Serial.println(F("ABSMAXT deactivated"));
  }

  // Is tank too hot?
  if (
      (TBoiler >= boilerSettings.MaxTankT) ||
      (warning_SMX_IsActivated && TBoiler >= boilerSettings.MaxTankT - 5))
  {
    if (!warning_SMX_IsActivated)
      Serial.println(F("SMX activated"));
    warning_SMX_IsActivated = true;

    jsonDoc["SMX"] = "Maximum tank temperature protection active";
    return false;
  }

  if (warning_SMX_IsActivated)
  {
    warning_SMX_IsActivated = false;

    jsonDoc.remove("SMX");
    Serial.println(F("SMX deactivated"));
  }

  return true;
}

// boiler & solar
void ProcessTemperatureSensors()
{
  jsonDoc.clear();

  float TSolar = solarT.getSolarPaneTemperature(jsonDoc);
  float TBottom = boilerT.getTemperature(TEMP_SENSOR_BOILER_BOTTOM); // Tank bottom
  float TTop = boilerT.getTemperature(TEMP_SENSOR_BOILER_TOP);       // Tank top

#ifdef SIMULATION_MODE
  TSolar = 60;
  TBottom = 30;
#endif
  //	Serial.print("T1 = "); Serial.println(TSolar);
  //	Serial.print("T2 = "); Serial.println(T2);
  //	Serial.print("T3 = "); Serial.println(T3);

  if (!isValidT(TSolar))
  {
    jsonDoc["SolarSensor"] = "Solar sensor temperature is not valid";
  }

  if (!isValidT(TBottom))
  {
    jsonDoc["TankBottomSensor"] = "Boiler tank bottom sensor temperature is not valid";
  }

  if (!isValidT(TTop))
  {
    jsonDoc["TankTopSensor"] = "Boiler tank top sensor temperature is not valid";
  }

  // Read sensor data
  if (!isValidT(TSolar))
  {
    solarPumpOn();
  }

  if (!isValidT(TTop))
  {
    TTop = TBottom;
  }

  if (!isValidT(TBottom) && isValidT(TTop))
  {
    TBottom = TTop;
  }

  // if (!isValidT(TF))
  // {
  //   burnerOff();
  // }

  float TBoiler = TTop;
  if (TBottom > TTop)
    TBoiler = TBottom;

  ////////////////// End of Read sensor data

  bool isSolarOk = CheckSolarPanels(TSolar);

  CheckBoilerTank(TBoiler);

  if (jsonDoc.size() > 0)
  {
    errorStatus.setValue("error");
    String json;
    serializeJson(jsonDoc, json);
    errorStatus.setJsonAttributes(json.c_str());

    jsonDoc.clear();
  }
  else
  {
    errorStatus.setValue("ok");
    errorStatus.setJsonAttributes(nullptr);
  }

  if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > boilerSettings.CollectorMinimumSwitchOnT) // T2 = Tank bottom
  {
    if (!isSolarPumpOn() && (TSolar >= TBottom + boilerSettings.CollectorSwitchOnTempDiff))
    {
      solarPumpOn();
    }
    else if (isSolarPumpOn() && (TSolar < TBottom + boilerSettings.CollectorSwitchOffTempDiff))
    {
      solarPumpOff();
    }
  }

  if ((!warning_SMX_IsActivated && !warning_EMOF_IsActivated) && isValidT(TTop))
  {
    if (boilerSettings.Mode == BOILER_MODE_SUMMER_POOL)
    {
      if (!poolPumpIsOn)
      {
        heatingCirculationPumpPumpOff();
      }
      else
      {
        if (TTop >= boilerSettings.PoolSwitchOnT)
        {
          heatingCirculationPumpOn();
        }
        else if (TTop < boilerSettings.PoolSwitchOffT)
        {
          heatingCirculationPumpPumpOff();
        }
      }
    }
  }

  solarTemperatureSensor.setValue(TSolar);
}

void initHeaterRelays()
{
  for (byte i = 0; i < HEATER_RELAY_COUNT; i++)
  {
    digitalWrite(heaterRelayPins[i], LOW);
    pinMode(heaterRelayPins[i], OUTPUT);
  }
}

// static void heaterRelaySetValue(byte id, bool value)
// {
// 	if (id < HEATER_RELAY_COUNT)
// 	{
// 		if (heaterRelayState[id] != value)
// 		{
// 			heaterRelayState[id] = value;

// 			// PublishHeaterRelayState(id, value);
// 		}
// 	}
// }
// static void processHeaterRelays()
// {
//   for (byte id = 0; id < HEATER_RELAY_COUNT; id++)
//   {
//     if (heaterRelayState[id])
//     {
//       digitalWrite(heaterRelayPins[id], LOW); // OPEN
//     }
//     else
//     {
//       digitalWrite(heaterRelayPins[id], HIGH); // CLOSE
//     }
//   }
// }

static void processPoolPumpState(byte id, bool value)
{
  if (id != 0)
    return;

  poolPumpIsOn = value;

  ProcessTemperatureSensors();
}
// bool checkAllHeaterRelaysAreOff()
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
// }
