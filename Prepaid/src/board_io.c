#include "board_io.h"
#include "delay.h"

sbit LED_1_PIN = P3^0;
sbit LED_2_PIN = P1^7;
sbit BUZZER_PIN = P3^4;
sbit RELAY_SET_1_PIN = P0^5;
sbit RELAY_SET_2_PIN = P0^4;
sbit RELAY_SET_3_PIN = P0^3;
sbit RELAY_RESET_1_PIN = P0^0;
sbit RELAY_RESET_2_PIN = P1^0;
sbit RELAY_RESET_3_PIN = P1^1;
sbit RS485_1_DE_PIN = P3^3;
sbit RS485_1_DE_ALT_PIN = P2^5;
sbit RS485_2_DE_PIN = P0^1;
sbit SHIFT_DATA_PIN = P2^1;
sbit SHIFT_CLOCK_PIN = P2^2;
sbit SHIFT_LATCH_PIN = P2^3;
sbit KEY_1_PIN = P3^5;
sbit KEY_2_PIN = P3^2;
sbit KEY_3_PIN = P3^1;
sbit IR_INPUT_PIN = P2^4;

void Board_RelaysOff(void)
{
    SFRS = 0;
    RELAY_SET_1_PIN = 0;
    RELAY_SET_2_PIN = 0;
    RELAY_SET_3_PIN = 0;
    RELAY_RESET_1_PIN = 0;
    RELAY_RESET_2_PIN = 0;
    RELAY_RESET_3_PIN = 0;
}

void Board_Init(void)
{
    Board_RelaysOff();
    BUZZER_PIN = 0;
    RS485_1_DE_PIN = 0;
    RS485_1_DE_ALT_PIN = 0;
    RS485_2_DE_PIN = 0;
    LED_1_PIN = 1;
    LED_2_PIN = 1;
    SHIFT_DATA_PIN = 0;
    SHIFT_CLOCK_PIN = 0;
    SHIFT_LATCH_PIN = 0;

    P00_PUSHPULL_MODE;
    P01_PUSHPULL_MODE;
    P03_PUSHPULL_MODE;
    P04_PUSHPULL_MODE;
    P05_PUSHPULL_MODE;
    P10_PUSHPULL_MODE;
    P11_PUSHPULL_MODE;
    P17_PUSHPULL_MODE;
    P21_PUSHPULL_MODE;
    P22_PUSHPULL_MODE;
    P23_PUSHPULL_MODE;
    P25_PUSHPULL_MODE;
    P30_PUSHPULL_MODE;
    P33_PUSHPULL_MODE;
    P34_PUSHPULL_MODE;

    P31_INPUT_MODE;
    P32_INPUT_MODE;
    P35_INPUT_MODE;
    P24_INPUT_MODE;

    P37_QUASI_MODE;
    P36_INPUT_MODE;
    P06_QUASI_MODE;
    P07_INPUT_MODE;
    P13_OPENDRAIN_MODE;
    P14_OPENDRAIN_MODE;
    SFRS = 0;
}

void Board_DelayMs(uint16_t milliseconds)
{
    while (milliseconds >= 1000)
    {
        Timer0_Delay(24000000UL, 1000, 1000);
        milliseconds -= 1000;
    }
    if (milliseconds != 0)
    {
        Timer0_Delay(24000000UL, milliseconds, 1000);
    }
}

void Board_SetLed(uint8_t led, uint8_t on)
{
    SFRS = 0;
    if (led == 1)
    {
        LED_1_PIN = on ? 0 : 1;
    }
    else if (led == 2)
    {
        LED_2_PIN = on ? 0 : 1;
    }
}

void Board_SetBuzzer(uint8_t on)
{
    SFRS = 0;
    BUZZER_PIN = on ? 1 : 0;
}

void Board_PulseRelay(uint8_t channel, uint8_t set_coil, uint16_t milliseconds)
{
    SFRS = 0;
    if (channel == 1)
    {
        if (set_coil) RELAY_SET_1_PIN = 1;
        else RELAY_RESET_1_PIN = 1;
    }
    else if (channel == 2)
    {
        if (set_coil) RELAY_SET_2_PIN = 1;
        else RELAY_RESET_2_PIN = 1;
    }
    else if (channel == 3)
    {
        if (set_coil) RELAY_SET_3_PIN = 1;
        else RELAY_RESET_3_PIN = 1;
    }
    else
    {
        return;
    }

    Board_DelayMs(milliseconds);
    Board_RelaysOff();
}

void Board_SetRs485Transmit(uint8_t port, uint8_t enable)
{
    SFRS = 0;
    if (port == 1)
    {
        RS485_1_DE_PIN = enable ? 1 : 0;
        RS485_1_DE_ALT_PIN = enable ? 1 : 0;
    }
    else if (port == 2)
    {
        RS485_2_DE_PIN = enable ? 1 : 0;
    }
}

void Board_Shift595(uint8_t value)
{
    uint8_t bit_index;

    SFRS = 0;
    SHIFT_LATCH_PIN = 0;
    for (bit_index = 0; bit_index < 8; bit_index++)
    {
        SHIFT_CLOCK_PIN = 0;
        SHIFT_DATA_PIN = (value & 0x80) ? 1 : 0;
        SHIFT_CLOCK_PIN = 1;
        value <<= 1;
    }
    SHIFT_CLOCK_PIN = 0;
    SHIFT_LATCH_PIN = 1;
    SHIFT_LATCH_PIN = 0;
}

uint8_t Board_KeyPressed(uint8_t key)
{
    SFRS = 0;
    if (key == 1) return (KEY_1_PIN == 0);
    if (key == 2) return (KEY_2_PIN == 0);
    if (key == 3) return (KEY_3_PIN == 0);
    return 0;
}

uint8_t Board_IrActive(void)
{
    SFRS = 2;
    return (IR_INPUT_PIN == 0);
}
