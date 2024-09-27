#ifndef _MAIN_H
#define _MAIN_H

#include <ProjectDefines.h>
#include <Arduino.h>
#include <TemperatureDS18B20.h>
#include <SolarTemperatureReader.h>
#include <HASwitchX.h>
#include <HATargetTemperatureX.h>

struct RoomTemperatureSensor
{
  const char *topic;            // MQTT topic for receiving temperature data
  float value;                  // Current temperature value
  HATargetTemperatureX *targetTemperature; // Target temperature managed by HA
  HASwitchX *relay;             // Pointer to the associated heating relay
};

// Arduino Mega -->  MAX31865
//     -------------------------- -
//     CS:   pin 5    -->  CS
//     MOSI: pin 51   -->  SDI
//     MISO: pin 50   -->  SDO
//     SCK:  pin 52   -->  SCLK

const byte CIRCULATING_PUMP_ON_DELAY_MINUTES = 5;

const int PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_SECONDS = 10;
const int PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC = PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_SECONDS * 2;

const int STORAGE_ADDRESS_BOILER_SETTINGS = 100;

const int STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS = 900;

const unsigned int ERR_T1 = 64;
const unsigned int ERR_T2 = 128;
const unsigned int ERR_T3 = 256;
const unsigned int ERR_TF = 512; // Furnace

const byte HEATER_RELAY_COUNT = 16;
const byte USED_HEATER_RELAY_COUNT = 13;

const byte BOILER_RELAY_COUNT = 4;
const byte BOILER_SENSOR_COUNT = 4;
const byte SOLAR_SENSOR_COUNT = 1;
const byte PRESSURE_SENSOR_COUNT = 1;
const byte ERROR_AND_WARNING_COUNT = 1;

const byte SETTING_COUNT = 10;
const byte TARGET_T_SENSOR_COUNT = 14;

const int TEMPERATURE_SENSOR_BOILER_BOTTOM = 0;
const int TEMPERATURE_SENSOR_BOILER_TOP = 1;
const int TEMPERATURE_SENSOR_FURNACE = 2;
// const int TEMPERATURE_SENSOR_RESERVE_1 = 3;

const byte DEVICE_COUNT = (USED_HEATER_RELAY_COUNT + TARGET_T_SENSOR_COUNT + BOILER_RELAY_COUNT + BOILER_SENSOR_COUNT + SOLAR_SENSOR_COUNT + PRESSURE_SENSOR_COUNT + ERROR_AND_WARNING_COUNT + SETTING_COUNT);

inline boolean isValidT(float T)
{
  return !isnan(T);
}

const uint8_t THERMOSTAT_MODE_OFF = 0;
const uint8_t THERMOSTAT_MODE_WINTER = 1;
const uint8_t THERMOSTAT_MODE_SUMMER = 2;
const uint8_t THERMOSTAT_MODE_SUMMER_POOL = 3;

// const byte BL_SOLAR_PUMP = 0;
// const byte BL_CIRC_PUMP = 1;
// const byte BL_FURNACE = 2;
// const byte BL_FURNACE_CIRC_PUMP = 3;

const byte T_SOLAR_PANEL_T = 0;
const byte T_TANK_BOTTOM = 1;
const byte T_TANK_TOP = 2;
// const byte T_FURNACE = 3;

const byte PIN_PRESSURE_SENSOR = A0;

const byte PIN_BLINKING_LED = LED_BUILTIN; // 13 in MEGA

#ifndef SIMULATION_MODE
const byte PIN_SD_CARD_SELECT = 4;
const byte PIN_MAX31865_SELECT = 5;
const byte PIN_ETHERNET_SS = 10;

const byte PIN_ONE_WIRE_BUS = 8; // 4.7K pullup

const byte PIN_HR_1 = 22;
const byte PIN_HR_2 = 24;
const byte PIN_HR_3 = 26;
const byte PIN_HR_4 = 28;

const byte PIN_HR_5 = 23;
const byte PIN_HR_6 = 25;
const byte PIN_HR_7 = 27;
const byte PIN_HR_8 = 29;

const byte PIN_HR_9 = 30;
const byte PIN_HR_10 = 32;
const byte PIN_HR_11 = 34;
const byte PIN_HR_12 = 36;

const byte PIN_HR_13 = 31;
const byte PIN_HR_14 = 33;
const byte PIN_HR_15 = 35;
const byte PIN_HR_16 = 37;

const byte PIN_BL_SOLAR_PUMP = 38;
const byte PIN_BL_CIRC_PUMP = 40;

const byte PIN_BL_OVEN_PUMP = 42;
const byte PIN_BL_OVEN_FAN = 44;

const byte PIN_BL_RESERVE1 = 39;
const byte PIN_BL_RESERVE2 = 41;
const byte PIN_BL_RESERVE3 = 43;
const byte PIN_BL_RESERVE4 = 45;
#else
const byte PIN_MAX31865_SELECT = D7;
const byte PIN_BL_SOLAR_PUMP = D7;
const byte PIN_BL_CIRC_PUMP = D7;
const byte PIN_ONE_WIRE_BUS = D8; // 4.7K pullup

const byte PIN_HR_1 = D8;
const byte PIN_HR_2 = D8;
const byte PIN_HR_3 = D8;
const byte PIN_HR_4 = D8;

const byte PIN_HR_5 = D8;
const byte PIN_HR_6 = D8;
const byte PIN_HR_7 = D8;
const byte PIN_HR_8 = D8;

const byte PIN_HR_9 = D8;
const byte PIN_HR_10 = D8;
const byte PIN_HR_11 = D8;
const byte PIN_HR_12 = D8;

const byte PIN_HR_13 = D8;
const byte PIN_HR_14 = D8;
const byte PIN_HR_15 = D8;
const byte PIN_HR_16 = D8;

#endif

#endif