#include "main.h"

#include <Adafruit_MAX31865.h>
#include <SolarTemperatureReader.h>
#include <HAHeatingX.h>
#include <HASettingX.h>
#include <HAModeX.h>
#include <HAErrorStatusX.h>
#include <HAPumpX.h>
#include "pressure_reader.h"

const byte TEMP_SENSOR_TANK_BOTTOM = 0;
const byte TEMP_SENSOR_TANK_TOP = 1;
const byte TEMP_SENSOR_ROOM = 2;
// const int TEMP_SENSOR_RESERVE_1 = 3;

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

extern HAPumpX *solarRecirculatingPump;
extern HAPumpX *heatingRecirculatingPump;
extern HABinarySensor *heatingRecirculatingPumpStarting;

extern HASettingX *settingCollectorSwitchOnTempDiff;
extern HASettingX *settingCollectorSwitchOffTempDiff;
extern HASettingX *settingCollectorEmergencySwitchOffT;
extern HASettingX *settingCollectorEmergencySwitchOnT;
extern HASettingX *settingCollectorAntifreezeT;
extern HASettingX *settingMaxTankT;
extern HASettingX *settingAbsoluteMaxTankT;
extern HASettingX *settingPoolSwitchOnT;
extern HASettingX *settingPoolSwitchOffT;

extern PressureReader *pressureReader;

extern String pool_pump_state;

extern unsigned long secondTicks;
unsigned long heatingCirculationPumpOnSeconds = 0;

// static void heatingCirculationPumpOnTimer();

static void solarPumpOn(float pressure)
{
    if (pressure >= 0.5f)
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

static bool canStartHearingCirculationPump()
{
    return secondTicks >= heatingCirculationPumpOnSeconds;
}

static void heatingCirculationPumpOn()
{
    Serial.println("Heating pump on requested");
    Serial.println(heatingCirculationPumpOnSeconds - secondTicks);

    if (heatingRecirculatingPump->isTurnedOn())
        return;

    if (controllerMode->getCurrentState() == THERMOSTAT_MODE_WINTER) // needs 5 min delay
    {
        if (heatingCirculationPumpOnSeconds == 0)
        {
            heatingCirculationPumpOnSeconds = secondTicks + CIRCULATING_PUMP_ON_DELAY_MINUTES * 60;
            heatingRecirculatingPumpStarting->setState(true);
        }
        else if (canStartHearingCirculationPump())
        {
            heatingCirculationPumpOnSeconds = 0;
            heatingRecirculatingPump->setOnOff(HIGH);
            heatingRecirculatingPumpStarting->setState(false);
        }
        // PublishBoilerRelayState(BL_CIRC_PUMP, false); // will publish standby mode
    }
    else
    {
        heatingRecirculatingPump->setOnOff(HIGH);
        heatingRecirculatingPumpStarting->setState(false);
    }
}

static void heatingCirculationPumpOff()
{
    heatingCirculationPumpOnSeconds = 0;

    heatingRecirculatingPump->setOnOff(LOW);
    heatingRecirculatingPumpStarting->setState(false);
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

static bool CheckSolarPanel(float TSolar, uint8_t &errors, float pressure)
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
        solarPumpOn(pressure);

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

    float pressure = pressureReader->getLastPressure();

    if (pressure < 0.5f)
        solarPumpOff();

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
        solarPumpOn(pressure);
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

    bool isSolarOk = CheckSolarPanel(TSolar, errors, pressure);

    CheckTank(TBoiler, errors);

    errorStatus->setStatus(max31865_fault, errors);

    if (isSolarOk && isValidT(TSolar) && isValidT(TBottom) && TSolar > SOLAR_COLLECTOR_CMN) // T2 = Tank bottom
    {
        if (!isSolarPumpOn() && (TSolar >= TBottom + settingCollectorSwitchOnTempDiff->getSettingValue()))
        {
            solarPumpOn(pressure);
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
            if (pool_pump_state != "on")
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

void processHeaterRelays()
{
    if (warning_MAXT_IsActivated || controllerMode->getCurrentState() != THERMOSTAT_MODE_WINTER)
    {
        return;
    }

    bool atLeastOneIsOpen = false;

    // Iterate through all sensors to find a relay
    for (int i = 0; i < MQTT_TEMPERATURE_SENSOR_COUNT; i++)
    {
        RoomTemperatureSensor *sensor = roomSensors[i];
        HAHeatingX *relay = sensor->relay;
        if (relay == NULL || sensor->targetTemperature == NULL)
            continue;

        float tempValue = sensor->value;
        float targetTemp = sensor->targetTemperature->getCurrentState().toFloat(); // Get current target temperature
        float hysteresis = 0.2f;                                                   // Define hysteresis value

        bool openValve = false;

        if (targetTemp > 16.0f)
        {
            if (isnan(tempValue))
            {
                openValve = true;
                Serial.println(F("Invalid room temperature"));
            }
            else if (tempValue < (targetTemp - hysteresis))
            {
                openValve = true;
            }
            else if (tempValue > (targetTemp + hysteresis))
            {
                openValve = false;
            }
        }
        else
        {
            relay->setDefaultState();
        }

        if (openValve)
            atLeastOneIsOpen = true;

        Serial.print(relay->uniqueId());
        Serial.print(F(", T: "));
        Serial.print(tempValue);
        Serial.print(F(", TT: "));
        Serial.print(targetTemp);
        Serial.print(F(", Open?: "));
        Serial.println(openValve);
        Serial.print(F(", IsOpen?: "));
        Serial.println(relay->isOpen());

        if (openValve != relay->isOpen())
        {
            relay->setOpenClose(openValve);
            // Serial.print(F("Heating Relay "));
            // Serial.print(relay->uniqueId());
            // Serial.println(openValve ? " opened" : " closed");
        }
    }

    if (atLeastOneIsOpen)
        heatingCirculationPumpOn();
    else
        heatingCirculationPumpOff();
}