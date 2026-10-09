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
void RS485_SendManualTest(uint8_t port);

#endif
