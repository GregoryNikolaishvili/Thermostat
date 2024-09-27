#ifndef MovingAverageFilter_h
#define MovingAverageFilter_h

#define MAX_DATA_POINTS 20

class MovingAverageFilter
{
public:
	MovingAverageFilter(unsigned int newDataPointsCount, int errorValue);

	int process(int value);
	int getCurrentValue();
	void reset();

private:
	long sum;
	int lastValue;
	int values[MAX_DATA_POINTS];
	unsigned int readingCount;
	unsigned int currentIdx; // k stores the index of the current array read to create a circular memory through the array
	int errorValue;
	unsigned int totalErrors;
protected:
	unsigned int dataPointsCount;
};
#endif
