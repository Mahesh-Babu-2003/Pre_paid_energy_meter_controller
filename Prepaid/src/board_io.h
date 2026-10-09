#ifndef BOARD_IO_H
#define BOARD_IO_H

#include "ms51_device.h"

void Board_Init(void);
void Board_DelayMs(uint16_t milliseconds);
void Board_SetLed(uint8_t led, uint8_t on);
void Board_SetBuzzer(uint8_t on);
void Board_RelaysOff(void);
void Board_PulseRelay(uint8_t channel, uint8_t set_coil, uint16_t milliseconds);
void Board_SetRs485Transmit(uint8_t port, uint8_t enable);
void Board_Shift595(uint8_t value);
uint8_t Board_KeyPressed(uint8_t key);
uint8_t Board_IrActive(void);

#endif
