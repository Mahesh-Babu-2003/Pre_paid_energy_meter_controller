#include "rs485_test.h"
#include "board_io.h"
#include "uart.h"

static char code port1_message[] = "PORT1 TEST\r\n";
static char code port2_message[] = "PORT2 TEST\r\n";

static void RS485_SendByte(uint8_t port, uint8_t value)
{
    SFRS = 0;
    if (port == 1)
    {
        SBUF_1 = value;
        while (TI_1 == 0) { }
        TI_1 = 0;
    }
    else
    {
        SBUF = value;
        while (TI == 0) { }
        TI = 0;
    }
}

void RS485_TestInit(void)
{
    IE &= 0x7F;
    P37_QUASI_MODE;
    P36_INPUT_MODE;
    P06_QUASI_MODE;
    P07_INPUT_MODE;
    SFRS = 0;
    UART_Open(24000000UL, UART1_Timer3, 9600);
    UART_Open(24000000UL, UART0_Timer1, 9600);
    Board_SetRs485Transmit(1, 0);
    Board_SetRs485Transmit(2, 0);
}

void RS485_Poll(RS485_TestResult *result)
{
    SFRS = 0;
    if (RI_1)
    {
        result->last_byte = SBUF_1;
        RI_1 = 0;
        result->last_port = 1;
        result->port1_rx_count++;
    }
    if (RI)
    {
        result->last_byte = SBUF;
        RI = 0;
        result->last_port = 2;
        result->port2_rx_count++;
    }
}

void RS485_SendManualTest(uint8_t port)
{
    char code *message;

    if (port == 1)
    {
        message = port1_message;
    }
    else if (port == 2)
    {
        message = port2_message;
    }
    else
    {
        return;
    }

    Board_SetRs485Transmit(port, 1);
    Board_DelayMs(2);
    while (*message != '\0')
    {
        RS485_SendByte(port, *message++);
    }
    Board_DelayMs(2);
    Board_SetRs485Transmit(port, 0);
}
