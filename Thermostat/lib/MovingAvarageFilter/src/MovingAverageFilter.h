#ifndef MovingAverageFilter_h
#define MovingAverageFilter_h

#define MAX_DATA_POINTS 20

class MovingAverageFilter
{
public:
	MovingAverageFilter(uint16_t newDataPointsCount, int16_t errorValue);

	int16_t process(int16_t value);
	int16_t getCurrentValue();
	void reset();

private:
	long sum;
	int16_t lastValue;
	int16_t values[MAX_DATA_POINTS];
	uint16_t readingCount;
	uint16_t currentIdx; // k stores the index of the current array read to create a circular memory through the array
	int16_t errorValue;
	uint16_t totalErrors;
protected:
	uint16_t dataPointsCount;
};
#endif
