#include <arduino.h>
#include <Time.h>					// http://www.pjrc.com/teensy/td_libs_Time.html


//Arduino Mega -->  MAX31865
//    -------------------------- -
//    CS:   pin 5    -->  CS
//    MOSI: pin 51   -->  SDI
//    MISO: pin 50   -->  SDO
//    SCK:  pin 52   -->  SCLK

const byte CIRCULATING_PUMP_ON_DELAY_MINUTES = 5;

#define PROCESS_INTERVAL_BOILER_TEMPERATURE_SENSOR_HALF_SEC 20

const int STORAGE_ADDRESS_BOILER_SETTINGS = 100;
const int STORAGE_ADDRESS_ROOM_SENSOR_SETTINGS = 300;
const int STORAGE_ADDRESS_DS18B20_ADDRESS_SETTINGS = 900;
const int STORAGE_ADDRESS_DATA = 1000;

const unsigned int ERR_GENERAL = 1;
const unsigned int ERR_DS18B20_SENSOR = 2;
const unsigned int ERR_EMOF = 4;
const unsigned int ERR_95_DEGREE = 8;
const unsigned int ERR_CMX = 16;
const unsigned int ERR_SMX = 32;

const unsigned int ERR_T1 = 64;
const unsigned int ERR_T2 = 128;
const unsigned int ERR_T3 = 256;
const unsigned int ERR_TF = 512; // Furnace


#define state_is_error_bit_set(__mask__) ((thermostatControllerState & (__mask__)) != 0)
//#define state_set_error_bit(__mask__) (thermostatState |= (__mask__))
//#define state_clear_error_bit(__mask__) (thermostatState &= ~(__mask__))

const byte HEATER_RELAY_COUNT = 16;
const byte BOILER_RELAY_COUNT = 4;
const byte BOILER_SENSOR_COUNT = 4;

const byte MAX_ROOM_SENSORS = 30;
const int T_UNDEFINED = 9999;

inline boolean isValidT(int T) { return T != T_UNDEFINED; }

//const char BOILER_MODE_COUNT = 4;

const char BOILER_MODE_OFF = 'N';
const char BOILER_MODE_SUMMER = 'S';
const char BOILER_MODE_SUMMER_POOL = 'P';
const char BOILER_MODE_WINTER = 'W';

const byte BL_SOLAR_PUMP = 0;
const byte BL_CIRC_PUMP = 1;
const byte BL_FURNACE = 2;
const byte BL_FURNACE_CIRC_PUMP = 3;

const byte T_SOLAR_PANEL_T = 0;
const byte T_TANK_BOTTOM = 1;
const byte T_TANK_TOP = 2;
const byte T_FURNACE = 3;

const byte PIN_MANUAL_MODE_LED = 3;
const byte PIN_SD_CARD_SELECT = 4;
const byte PIN_MAX31865_SELECT = 5;
const byte PIN_BLINKING_LED = LED_BUILTIN; // 13 in MEGA
const byte PIN_ONE_WIRE_BUS = 8;	// 4.7K pullup

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

struct RoomSensorSettingStructure {
	int id;
	int targetT;
	byte relayId;
};


struct BoilerSettingStructure {
	char Mode;

	int CollectorSwitchOnTempDiff;
	int CollectorSwitchOffTempDiff;

	int EmergencyCollectorSwitchOffT; // EMOF, Collector maximum switch - off temperature 130
	int EmergencyCollectorSwitchOnT; // EMON, Collector	maximum switch - on temperature 120
	int CollectorCoolingT; // CMX, Maximum temperature of collector	(Collector cooling function) 110
	int MaxTankT; // SMX Maximum temperature of tank
	
	int PoolSwitchOnT;
	int PoolSwitchOffT;

	int BackupHeatingTS1_Start;
	int BackupHeatingTS1_End;
	int BackupHeatingTS1_SwitchOnT;
	int BackupHeatingTS1_SwitchOffT;

	int BackupHeatingTS2_Start;
	int BackupHeatingTS2_End;
	int BackupHeatingTS2_SwitchOnT;
	int BackupHeatingTS2_SwitchOffT;

	int BackupHeatingTS3_Start;
	int BackupHeatingTS3_End;
	int BackupHeatingTS3_SwitchOnT;
	int BackupHeatingTS3_SwitchOffT;
};



const byte LINEAR_REGRESSION_POINT_COUNT = 5;

class Temperature
{
private:
	byte cnt;
	byte idx;
	int lastValue;
	int oldValues[LINEAR_REGRESSION_POINT_COUNT];
	time_t lastReadingTime;
public:
	Temperature();
	void clear();

	int getValue();
	void addValue(const int value);
	char getTrend();
	time_t getLastReadingTime();
};
