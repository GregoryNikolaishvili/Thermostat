#include "main.h"

#include <Adafruit_MAX31865.h>
#include <SolarTemperatureReader.h>
#include <TimeAlarms.h>
#include <HAHeatingX.h>
#include <HASettingX.h>
#include <HAModeX.h>
#include <HAErrorStatusX.h>
#include <HAPumpX.h>

const byte TEMP_SENSOR_TANK_BOTTOM = 0;
const byte TEMP_SENSOR_TANK_TOP = 1;
const byte TEMP_SENSOR_ROOM = 2;
// const int TEMP_SENSOR_RESERVE_1 = 3;

AlarmID_t heatingCirculationPumpStartingAlarm = 0xFF;

extern HAModeX *controllerMode;
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
extern HAErrorStatusX *errorStatus;

extern HAPumpX *heatingRecirculatingPump;
extern HAPumpX *solarRecirculatingPump;

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

    if (controllerMode->getCurrentState() == THERMOSTAT_MODE_WINTER) // needs 5 min delay
    {
        heatingCirculationPumpStartingAlarm = Alarm.alarmOnce(elapsedSecsToday(now()) + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60, heatingCirculationPumpOnTimer);

        // PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
    }
    else
    {
        heatingRecirculatingPump->setOnOff(HIGH);
    }
}

static void heatingCirculationPumpOff()
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
        HAHeatingX *relay = roomSensors[i]->relay;
        if (relay != NULL)
        {
            relay->setOpenClose(true);
        }
    }
}

static void AllHeaterRelaysToDefault()
{
    Serial.println("All heater relays to default");
    for (byte i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        HAHeatingX *relay = roomSensors[i]->relay;
        if (relay != NULL)
        {
            relay->setDefaultState();
        }
    }
}

void initThermostat(int16_t mode)
{
    if (mode != THERMOSTAT_MODE_WINTER)
    {
        AllHeaterRelaysToDefault();
        heatingCirculationPumpOff();
        return;
    }
}

static bool CheckSolarPanel(float TSolar, uint8_t &errors)
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

        errors |= ERR_EMOF;
        // jsonBuilder.addKeyValue(F("EMOF"), F("Max collector temperature protection is active"));
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

        errors |= ERR_CFR;
        // jsonBuilder.addKeyValue(F("CFR"), F("Frost protection of collector is active"));
        return false;
    }

    if (warning_CFR_IsActivated)
    {
        warning_CFR_IsActivated = false;
        Serial.println(F("CFR deactivated"));
    }

    return true;
}

bool CheckTank(float TTank, uint8_t &errors)
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

        errors |= ERR_ABSMAX_T;
        // jsonBuilder.addKeyValue(F("ABSMAXT"), F("Absolute maximum tank temperature protection is active"));
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

        errors |= ERR_SMX;
        // jsonBuilder.addKeyValue(F("SMX"), F("Maximum tank temperature protection is active"));
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
    //    jsonBuilder.addKeyValue(F("MAX31865"), (int)fault);

    uint8_t max31865_fault = 0;
    uint8_t errors = 0;

    float TSolar = solarT->getSolarPaneTemperature(max31865_fault);
    float TBottom = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_BOTTOM); // Tank bottom
    float TTop = tankTemperatures.getTemperature(TEMP_SENSOR_TANK_TOP);       // Tank top
    float TRoom = tankTemperatures.getTemperature(TEMP_SENSOR_ROOM);

#ifdef SIMULATION_MODE
    TSolar = 60;
    TBottom = 30;
#endif
    Serial.print(F("T1 = "));
    Serial.println(TSolar);
    Serial.print(F("T2 = "));
    Serial.println(TBottom);
    Serial.print(F("T3 = "));
    Serial.println(TTop);
   
    if (!isValidT(TSolar))
    {
        errors |= ERR_SOLAR_T;
        // jsonBuilder.addKeyValue(F("SolarPanel"), F("Solar sensor temperature is invalid"));
    }

    if (!isValidT(TBottom))
    {
        errors |= ERR_TANK_BOTTOM_T;
        // jsonBuilder.addKeyValue(F("TankBottom"), F("Tank bottom temperature is invalid"));
    }

    if (!isValidT(TTop))
    {
        errors |= ERR_TANK_TOP_T;
        // jsonBuilder.addKeyValue(F("TankTop"), F("Tank top temperature is invalid"));
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

    bool isSolarOk = CheckSolarPanel(TSolar, errors);

    CheckTank(TBoiler, errors);

    errorStatus->setStatus(max31865_fault, errors);

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
        if (controllerMode->getCurrentState() == THERMOSTAT_MODE_SUMMER_POOL)
        {
            if (pool_pump_state != "On")
            {
                heatingCirculationPumpOff();
            }
            else
            {
                if (TTop >= settingPoolSwitchOnT->getSettingValue())
                {
                    heatingCirculationPumpOn();
                }
                else if (TTop < settingPoolSwitchOffT->getSettingValue())
                {
                    heatingCirculationPumpOff();
                }
            }
        }
    }

    solarTemperatureSensor->setValue(TSolar);
    tankBottomTemperatureSensor->setValue(TBottom);
    tankTopTemperatureSensor->setValue(TTop);
    roomTemperatureSensor->setValue(TRoom);
}
