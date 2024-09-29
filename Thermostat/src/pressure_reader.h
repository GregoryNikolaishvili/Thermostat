#ifndef _PRESSURE_READER_H
#define _PRESSURE_READER_H

#include <ProjectDefines.h>
#include <Arduino.h>
#include <ArduinoHA.h>
#include <MovingAverageFilter.h>

#define PRESSURE_ERR_VALUE 999.9

class PressureReader
{
public:
	PressureReader(HASensorNumber *pressureSensor);

	void processPressureSensor();
	float getPressure();

	inline float getLastPressure()
	{
		return _lastValue;
	}

private:
	float _lastValue;
	HASensorNumber *_pressureSensor;
	MovingAverageFilter *_pressureAvg;
};

#endif