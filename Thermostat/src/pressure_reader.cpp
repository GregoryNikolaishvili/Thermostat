#include "pressure_reader.h"
#include "main.h"

PressureReader::PressureReader(HASensorNumber *pressureSensor)
{
  _lastValue = 1.5f;
  _pressureSensor = pressureSensor;

  _pressureAvg = new MovingAverageFilter(5, PRESSURE_ERR_VALUE);
  pinMode(PIN_PRESSURE_SENSOR, INPUT);
}

void PressureReader::processPressureSensor()
{
#ifndef SIMULATION_MODE
  int value = analogRead(PIN_PRESSURE_SENSOR) - 47;
  if (value < 0)
    value = 0;
  // Serial.print("Pressure voltage");
  // Serial.println(value);
  
  // Sensor output voltage
  float V = value * 5.00 / 1024; // 5.00 is reference voltage
  // Calculate water pressure
  float P = V * 498.0 / 3.9; // 500 = 500 KPa = 5 Bar, calibrated July 23, 2021)
  value = round(P);          // KPa, i.e 210 Kpa from 2.1 Bar
  // Returns pressure * 10 in Bars

  //	Serial.print(F("Pressure = "));
  //	Serial.print(P);
  //	Serial.print(F(" KPa, Value = "));
  //	Serial.print(value);
  //	Serial.print(F(", Voltage = "));
  //	Serial.print(V);
  //  Serial.print(F("V, P10 = "));
  //  Serial.println(pressure10);

  int newValue = _pressureAvg->process(value);
#else
  int newValue = _pressureAvg->process(25);
#endif
  _pressureSensor->setValue(newValue / 100.0f);
}

// Returns pressure in Bars
float PressureReader::getPressure()
{
  return _pressureAvg->getCurrentValue() / 10.0;
}