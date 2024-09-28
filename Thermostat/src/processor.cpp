#include "main.h"

#include <Adafruit_MAX31865.h>
#include <SolarTemperatureReader.h>
#include <TimeAlarms.h>
#include <HASwitchX.h>
#include <HASettingX.h>
#include <TinyJsonBuilder.h>

const byte TEMP_SENSOR_TANK_BOTTOM = 0;
const byte TEMP_SENSOR_TANK_TOP = 1;
const byte TEMP_SENSOR_ROOM = 2;
// const int TEMP_SENSOR_RESERVE_1 = 3;

AlarmID_t heatingCirculationPumpStartingAlarm = 0xFF;

bool isTankOverheated = false;

// Define a buffer to hold the JSON string
char jsonBuffer[512]; // Adjust size based on expected content
TinyJsonBuilder jsonBuilder(jsonBuffer, sizeof(jsonBuffer));

extern HASelect *modeSelect;
extern RoomTemperatureSensor *roomSensors[MQTT_TEMPERATURE_SENSOR_COUNT];

// Warning conditions
bool warning_EMOF_IsActivated = false;
bool warning_CFR_IsActivated = false;
bool warning_SMX_IsActivated = false;
bool warning_MAXT_IsActivated = false;

extern SolarTemperatureReader *solarT;
extern TemperatureDS18B20 tankTemperatures;

extern HASensorNumber *solarTemperatureSensor;
extern HASensorNumber *tankTopTemperatureSensor;
extern HASensorNumber *tankBottomTemperatureSensor;
extern HASensorNumber *roomTemperatureSensor;
extern HASensor *errorStatus;

extern HASwitchX *heatingRecirculatingPump;
extern HASwitchX *solarRecirculatingPump;

extern HASettingX *settingCollectorSwitchOnTempDiff;
extern HASettingX *settingCollectorSwitchOffTempDiff;
extern HASettingX *settingCollectorEmergencySwitchOffT;
extern HASettingX *settingCollectorEmergencySwitchOnT;
extern HASettingX *settingCollectorAntifreezeT;
extern HASettingX *settingMaxTankT;
extern HASettingX *settingAbsoluteMaxTankT;
extern HASettingX *settingPoolSwitchOnT;
extern HASettingX *settingPoolSwitchOffT;

extern String pool_pump_state;

static void heatingCirculationPumpOnTimer();

static void solarPumpOn()
{
    solarRecirculatingPump->setOnOff(HIGH);
}

void solarPumpOff()
{
    solarRecirculatingPump->setOnOff(LOW);
}

bool isSolarPumpOn()
{
    return solarRecirculatingPump->isTurnedOn();
}

boolean isHeatingCirculationPumpStarting()
{
    return heatingCirculationPumpStartingAlarm < 0xFF;
}

static void heatingCirculationPumpOn()
{
    if (isHeatingCirculationPumpStarting() || heatingRecirculatingPump->isTurnedOn())
        return;

    if (modeSelect->getCurrentState() == THERMOSTAT_MODE_WINTER) // needs 5 min delay
    {
        heatingCirculationPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, heatingCirculationPumpOnTimer);

        // PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
    }
    else
    {
        heatingRecirculatingPump->setOnOff(HIGH);
    }
}

static void heatingCirculationPumpPumpOff()
{
    if (heatingCirculationPumpStartingAlarm < 0xFF)
    {
        Alarm.free(heatingCirculationPumpStartingAlarm);
        heatingCirculationPumpStartingAlarm = 0xFF;
    }

    heatingRecirculatingPump->setOnOff(LOW);
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
    Serial.println("All heater relays ON");
    for (byte i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        HASwitchX *relay = roomSensors[i]->relay;
        if (relay != NULL)
        {
            relay->setOnOff(HIGH);
        }
    }
}

static void AllHeaterRelaysToDefault()
{
    Serial.println("All heater relays to default");
    for (byte i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        HASwitchX *relay = roomSensors[i]->relay;
        if (relay != NULL)
        {
            relay->setDefaultState();
        }
    }
}

void initThermostat()
{
    if (modeSelect->getCurrentState() != THERMOSTAT_MODE_WINTER)
    {
        AllHeaterRelaysToDefault();
        return;
    }
}

static bool CheckSolarPanel(float TSolar, TinyJsonBuilder &jsonBuilder)
{
    if (!isValidT(TSolar))
        return false;

    // Is solar collector too hot?
    if (
        (TSolar >= settingCollectorEmergencySwitchOffT->getSettingValue()) // 140
        ||
        (warning_EMOF_IsActivated && TSolar >= settingCollectorEmergencySwitchOnT->getSettingValue()))
    {
        solarPumpOff();

        if (!warning_EMOF_IsActivated)
            Serial.println(F("EMOF activated"));
        warning_EMOF_IsActivated = true;

        //jsonBuilder.addKeyValue(F("EMOF"), F("Max collector temperature protection is active"));
        return false;
    }

    if (warning_EMOF_IsActivated)
    {
        warning_EMOF_IsActivated = false;
        Serial.println(F("EMOF deactivated"));
    }

    // Is solar collector too cold?
    if (TSolar <= settingCollectorAntifreezeT->getSettingValue()) // 4
    {
        solarPumpOn();

        if (!warning_CFR_IsActivated)
        {
            warning_CFR_IsActivated = true;
            Serial.println(F("CFR activated"));
        }

        //jsonBuilder.addKeyValue(F("CFR"), F("Frost protection of collector is active"));
        return false;
    }

    if (warning_CFR_IsActivated)
    {
        warning_CFR_IsActivated = false;
        Serial.println(F("CFR deactivated"));
    }

    return true;
}

bool CheckTank(float TTank, TinyJsonBuilder &jsonBuilder)
{
    if (!isValidT(TTank))
        return false;

    // Is tank super hot?
    int absoluteMaxT = settingAbsoluteMaxTankT->getSettingValue();
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

        //jsonBuilder.addKeyValue(F("ABSMAXT"), F("Absolute maximum tank temperature protection is active"));
        return false;
    }

    if (warning_MAXT_IsActivated)
    {
        warning_MAXT_IsActivated = false;
        Serial.println(F("ABSMAXT deactivated"));
    }

    // Is tank too hot?
    int maxTankT = settingMaxTankT->getSettingValue();
    if ((TTank >= maxTankT) ||
        (warning_SMX_IsActivated && TTank >= maxTankT - 5))
    {
        if (!warning_SMX_IsActivated)
            Serial.println(F("SMX activated"));
        warning_SMX_IsActivated = true;

        //jsonBuilder.addKeyValue(F("SMX"), F("Maximum tank temperature protection is active"));
        return false;
    }

    if (warning_SMX_IsActivated)
    {
        warning_SMX_IsActivated = false;
        Serial.println(F("SMX deactivated"));
    }

    return true;
}

// boiler & solar
void ProcessTemperatureSensors()
{
    jsonBuilder.reset();
    
    float TSolar = solarT->getSolarPaneTemperature(jsonBuilder);
    float TBottom = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_BOTTOM); // Tank bottom
    float TTop = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_TOP);       // Tank top
    float TRoom = tankTemperatures.getTemperature(TEMP_SENSOR_ROOM);

#ifdef SIMULATION_MODE
    TSolar = 60;
    TBottom = 30;
#endif
    	Serial.print(F("T1 = ")); Serial.println(TSolar);
    	Serial.print(F("T2 = ")); Serial.println(TBottom);
    	Serial.print(F("T3 = ")); Serial.println(TTop);

    if (!isValidT(TSolar))
    {
        //jsonBuilder.addKeyValue(F("SolarPanel"), F("Solar sensor temperature is invalid"));
    }

    if (!isValidT(TBottom))
    {
        //jsonBuilder.addKeyValue(F("TankBottom"), F("Tank bottom temperature is invalid"));
    }

    if (!isValidT(TTop))
    {
        //jsonBuilder.addKeyValue(F("TankTop"), F("Tank top temperature is invalid"));
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

    bool isSolarOk = CheckSolarPanel(TSolar, jsonBuilder);

    CheckTank(TBoiler, jsonBuilder);

    if (!jsonBuilder.isEmpty())
    {
        errorStatus->setValue("error");
        //const char *json = jsonBuilder.getJson();
        //errorStatus->setJsonAttributes(json);
        jsonBuilder.reset();
        //Serial.println(json);
    }
    else
    {
        errorStatus->setValue("ok");
        errorStatus->setJsonAttributes(nullptr);
    }

    if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > SOLAR_COLLECTOR_CMN) // T2 = Tank bottom
    {
        if (!isSolarPumpOn() && (TSolar >= TBottom + settingCollectorSwitchOnTempDiff->getSettingValue()))
        {
            solarPumpOn();
        }
        else if (isSolarPumpOn() && (TSolar < TBottom + settingCollectorSwitchOffTempDiff->getSettingValue()))
        {
            solarPumpOff();
        }
    }

    if ((!warning_SMX_IsActivated && !warning_EMOF_IsActivated) && isValidT(TTop))
    {
        if (modeSelect->getCurrentState() == THERMOSTAT_MODE_SUMMER_POOL)
        {
            if (pool_pump_state != "On")
            {
                heatingCirculationPumpPumpOff();
            }
            else
            {
                if (TTop >= settingPoolSwitchOnT->getSettingValue())
                {
                    heatingCirculationPumpOn();
                }
                else if (TTop < settingPoolSwitchOffT->getSettingValue())
                {
                    heatingCirculationPumpPumpOff();
                }
            }
        }
    }

    solarTemperatureSensor->setValue(TSolar);
    tankBottomTemperatureSensor->setValue(TBottom);
    tankTopTemperatureSensor->setValue(TTop);
    roomTemperatureSensor->setValue(TRoom);
}
