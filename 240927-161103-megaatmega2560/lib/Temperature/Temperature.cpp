#include "Temperature.h"

Temperature::Temperature(unsigned int newDataPointsCount) : MovingAverageFilter(newDataPointsCount, T_UNDEFINED)
{
	roundingTo1 = true;
	clear();
}

Temperature::Temperature(unsigned int newDataPointsCount, bool rounding): MovingAverageFilter(newDataPointsCount, T_UNDEFINED)
{
	roundingTo1 = false;
	clear();
}

void Temperature::clear()
{
	currentValueRounded = T_UNDEFINED;
	trend = '=';

	MovingAverageFilter::reset();
}

int Temperature::process(const int value)
{
	int oldValueRounded = roundT(MovingAverageFilter::getCurrentValue()); // round to 1 degree

	int output = MovingAverageFilter::process(value);

	currentValueRounded = roundT(output); // round to 1 degree

	if (currentValueRounded == T_UNDEFINED)
		return currentValueRounded;

	if (currentValueRounded < oldValueRounded)
	{
		trend = '-';
	}
	else
		if (currentValueRounded > oldValueRounded)
		{
			trend = '+';
		}
		else
		{
			trend = '=';
		}

	return currentValueRounded;
}

int Temperature::getCurrentValue()
{
	return currentValueRounded;
}

char Temperature::getTrend()
{
	return trend;
}

int Temperature::roundT(int T)
{
	if (!roundingTo1)
		return T;

	if (T == T_UNDEFINED)
		return T;

	return round(T / 10) * 10;
}
