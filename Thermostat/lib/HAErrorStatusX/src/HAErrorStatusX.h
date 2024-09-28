#ifndef _HAERROR_STATUSX_H
#define _HAERROR_STATUSX_H

#include <ProjectDefines.h>
#include <ArduinoHA.h>

class HAErrorStatusX : public HASensor
{
public:
    HAErrorStatusX();

    void setStatus(uint8_t max31865_fault, uint8_t errors);

private:
    uint8_t _max31865_fault;
    uint8_t _errors;
};

#endif
