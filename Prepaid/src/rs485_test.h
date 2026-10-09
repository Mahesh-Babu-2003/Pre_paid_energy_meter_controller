#ifndef RS485_TEST_H
#define RS485_TEST_H

#include "ms51_device.h"

typedef struct
{
    uint8_t port1_rx_count;
    uint8_t port2_rx_count;
    uint8_t last_port;
    uint8_t last_byte;
} RS485_TestResult;

void RS485_TestInit(void);
void RS485_Poll(RS485_TestResult *result);
/* Meter register index 0 is holding register 40008; 9 is 40017. */
uint16_t RS485_GetMeterRegister(uint8_t meter_register_index);
uint8_t RS485_IsMeterDataValid(void);
uint8_t RS485_GetRelayState(uint8_t relay);

#endif
