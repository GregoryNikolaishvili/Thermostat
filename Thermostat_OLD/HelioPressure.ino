MovingAverageFilter* helioPressureValue;

int getHelioPressure10()
{
	return helioPressureValue->getCurrentValue() / 10; // 0.1 precision is enough
}

void InitHelioPressureSensor()
{
	helioPressureValue = new MovingAverageFilter(5, 1000);
}

// Returns pressure * 10 in Bars
void ProcessHelioPressure(bool &publishError)
{
	int value = analogRead(PIN_PRESSURE_SENSOR);
	// Sensor output voltage
	float V = value * 5.00 / 1024;  // 5.00 is reference voltage
	//Calculate water pressure 
	float P = V * 498.0 / 3.9;   // 500 = 500 KPa = 5 Bar, calibrated July 23, 2021)
	value = round(P); // KPa, i.e 210 Kpa fro 2.1 Bar

//	Serial.print("Pressure = ");
//	Serial.print(P);
//	Serial.print(" KPa, Value = ");
//	Serial.print(value);
//	Serial.print(", Voltage = ");
//	Serial.print(V);
//  Serial.print("V, P10 = ");
//  Serial.println(pressure10);

	int oldValue = helioPressureValue->getCurrentValue();
	int newValue = helioPressureValue->process(value);

	if (value < 100 || value > 300)
	{
		if (!warning_HelioPressure_IsActivated)
		{
			publishError = true;
			warning_HelioPressure_IsActivated = true;
		}
	}
	else
	{
		if (warning_HelioPressure_IsActivated)
		{
			publishError = true;
			warning_HelioPressure_IsActivated = false;
		}
	}

	if (oldValue / 10 != newValue / 10) // 0.1 precision is enough
	{
		PublishHelioPressure();
	}
}

