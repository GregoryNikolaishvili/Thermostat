#include "main.h"

#include <Adafruit_MAX31865.h>
#include <SolarTemperatureReader.h>
#include <ArduinoJson.h>
#include <TimeAlarms.h>
#include <HASwitchX.h>
#include <HASettingX.h>

const int TEMP_SENSOR_TANK_BOTTOM = 0;
const int TEMP_SENSOR_TANK_TOP = 1;
const int TEMP_SENSOR_ROOM = 2;
// const int TEMP_SENSOR_RESERVE_1 = 3;

AlarmID_t heatingCirculationPumpStartingAlarm = 0xFF;

bool isTankOverheated = false;

JsonDocument jsonDoc;

extern HASelect modeSelect;
extern RoomTemperatureSensor roomSensors[];
extern int MQTT_TEMPERATURE_SENSOR_COUNT;
extern String pool_pump_state;

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;

SolarTemperatureReader solarT(PIN_MAX31865_SELECT);
extern TemperatureDS18B20 tankTemperatures;

extern HASensorNumber solarTemperatureSensor;
extern HASensorNumber tankTopTemperatureSensor;
extern HASensorNumber tankBottomTemperatureSensor;
extern HASensor errorStatus;

extern HASwitchX heatingRecirculatingPump;
extern HASwitchX solarRecirculatingPump;

extern HASettingX settingCollectorSwitchOnTempDiff;
extern HASettingX settingCollectorSwitchOffTempDiff;
extern HASettingX settingCollectorEmergencySwitchOffT;
extern HASettingX settingCollectorEmergencySwitchOnT;
extern HASettingX settingCollectorMinimumSwitchOnT;
extern HASettingX settingCollectorAntifreezeT;
extern HASettingX settingMaxTankT;
extern HASettingX settingAbsoluteMaxTankT;
extern HASettingX settingPoolSwitchOnT;
extern HASettingX settingPoolSwitchOffT;

static void heatingCirculationPumpOnTimer();

static void solarPumpOn()
{
    solarRecirculatingPump.setOnOff(HIGH);
}

void solarPumpOff()
{
    solarRecirculatingPump.setOnOff(LOW);
}

bool isSolarPumpOn()
{
    return solarRecirculatingPump.isTurnedOn();
}

boolean isHeatingCirculationPumpStarting()
{
    return heatingCirculationPumpStartingAlarm < 0xFF;
}

static void heatingCirculationPumpOn()
{
    if (isHeatingCirculationPumpStarting() || heatingRecirculatingPump.isTurnedOn())
        return;

    if (modeSelect.getCurrentState() == THERMOSTAT_MODE_WINTER) // needs 5 min delay
    {
        heatingCirculationPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, heatingCirculationPumpOnTimer);

        // PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
    }
    else
    {
        heatingRecirculatingPump.setOnOff(HIGH);
    }
}

static void heatingCirculationPumpPumpOff()
{
    if (heatingCirculationPumpStartingAlarm < 0xFF)
    {
        Alarm.free(heatingCirculationPumpStartingAlarm);
        heatingCirculationPumpStartingAlarm = 0xFF;
    }

    heatingRecirculatingPump.setOnOff(LOW);
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

static void AllHeaterRelaysOn()
{
    for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        HASwitchX *relay = roomSensors[i].relay;
        if (relay != NULL)
        {
            relay->setOnOff(HIGH);
        }
    }
}

static void AllHeaterRelaysToDefault()
{
    for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        HASwitchX *relay = roomSensors[i].relay;
        if (relay != NULL)
        {
            relay->setDefaultState();
        }
    }
}

void initThermostat()
{
    if (modeSelect.getCurrentState() != THERMOSTAT_MODE_WINTER)
    {
        AllHeaterRelaysToDefault();
        return;
    }
}

static bool CheckSolarPanels(int TSolar)
{
    if (!isValidT(TSolar))
        return false;

    // Is solar collector too hot?
    if (
        (TSolar >= settingCollectorEmergencySwitchOffT.getSettingValue()) // 140
        ||
        (warning_EMOF_IsActivated && TSolar >= settingCollectorEmergencySwitchOnT.getSettingValue()))
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
    if (TSolar <= settingCollectorAntifreezeT.getSettingValue()) // 4
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

bool CheckTank(int TTank)
{
    if (!isValidT(TTank))
        return false;

    // Is tank super hot?
    int absoluteMaxT = settingAbsoluteMaxTankT.getSettingValue();
    if (
        (TTank >= absoluteMaxT) // 95 degree is absolute max for boiler tank
        ||
        (warning_MAXT_IsActivated && TTank >= absoluteMaxT - 5))
    {
        // Try to cool down boiler with all means
        AllHeaterRelaysOn();        // 100% open
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
    int maxTankT = settingMaxTankT.getSettingValue();
    if ((TTank >= maxTankT) ||
        (warning_SMX_IsActivated && TTank >= maxTankT - 5))
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
    float TBottom = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_BOTTOM); // Tank bottom
    float TTop = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_TOP);       // Tank top

#ifdef SIMULATION_MODE
    TSolar = 60;
    TBottom = 30;
#endif
    //	Serial.print("T1 = "); Serial.println(TSolar);
    //	Serial.print("T2 = "); Serial.println(T2);
    //	Serial.print("T3 = "); Serial.println(T3);

    if (!isValidT(TSolar))
    {
        jsonDoc["SolarPanel"] = "Solar sensor temperature is invalid";
    }

    if (!isValidT(TBottom))
    {
        jsonDoc["TankBottom"] = "Tank bottom temperature is invalid";
    }

    if (!isValidT(TTop))
    {
        jsonDoc["TankTop"] = "Tank top temperature is invalid";
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

    CheckTank(TBoiler);

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

    if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > settingCollectorMinimumSwitchOnT.getSettingValue()) // T2 = Tank bottom
    {
        if (!isSolarPumpOn() && (TSolar >= TBottom + settingCollectorSwitchOnTempDiff.getSettingValue()))
        {
            solarPumpOn();
        }
        else if (isSolarPumpOn() && (TSolar < TBottom + settingCollectorSwitchOffTempDiff.getSettingValue()))
        {
            solarPumpOff();
        }
    }

    if ((!warning_SMX_IsActivated && !warning_EMOF_IsActivated) && isValidT(TTop))
    {
        if (modeSelect.getCurrentState() == THERMOSTAT_MODE_SUMMER_POOL)
        {
            if (pool_pump_state != "On")
            {
                heatingCirculationPumpPumpOff();
            }
            else
            {
                if (TTop >= settingPoolSwitchOnT.getSettingValue())
                {
                    heatingCirculationPumpOn();
                }
                else if (TTop < settingPoolSwitchOffT.getSettingValue())
                {
                    heatingCirculationPumpPumpOff();
                }
            }
        }
    }

    solarTemperatureSensor.setValue(TSolar);
}
