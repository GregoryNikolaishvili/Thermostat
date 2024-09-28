#ifndef _HAMODEX_H
#define _HAMODEX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HAModeX : public HASelect
{
public:
    HAModeX(int16_t eepromAddress);
    
    inline void SaveModeToEeprom(int16_t mode)
    {
        writeIntToEEPROM(mode);
    }

private:
    int16_t _eepromAddress;

    int16_t readIntFromEEPROM();
    void writeIntToEEPROM(int16_t value);
};

#endif
