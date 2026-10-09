#include "interface_test.h"
#include "board_io.h"
#include "i2c_probe.h"
#include "lcd_595.h"
#include "rs485_test.h"

static I2C_TestResult i2c_result;
static RS485_TestResult rs485_result;
static uint8_t previous_key_1;
static uint8_t previous_key_2;
static uint8_t previous_key_3;
static uint8_t screen_page;
static uint16_t screen_timer;

static void InterfaceTest_RunOutputs(void)
{
    uint8_t channel;

    LCD_SetCursor(0, 0);
    LCD_WriteText("OUTPUT TEST      ");
    for (channel = 1; channel <= 2; channel++)
    {
        Board_SetLed(channel, 1);
        Board_DelayMs(180);
        Board_SetLed(channel, 0);
        Board_DelayMs(100);
    }

    Board_SetBuzzer(1);
    Board_DelayMs(120);
    Board_SetBuzzer(0);
    Board_DelayMs(80);
    Board_SetBuzzer(1);
    Board_DelayMs(120);
    Board_SetBuzzer(0);

    for (channel = 1; channel <= 3; channel++)
    {
        Board_PulseRelay(channel, 1, 120);
        Board_DelayMs(80);
        Board_PulseRelay(channel, 0, 120);
        Board_DelayMs(80);
    }
    Board_RelaysOff();
}

static void InterfaceTest_ShowStatus(void)
{
    LCD_SetCursor(0, 0);
    LCD_WriteText("RTC:");
    LCD_WriteChar(i2c_result.rtc_read_ok ? 'Y' : 'N');
    LCD_WriteText(" EE:");
    LCD_WriteChar(i2c_result.eeprom_read_ok ? 'Y' : 'N');
    LCD_WriteText(" IR:");
    LCD_WriteChar(Board_IrActive() ? '1' : '0');

    LCD_SetCursor(1, 0);
    LCD_WriteText("4851:");
    LCD_WriteHex(rs485_result.port1_rx_count);
    LCD_WriteText(" 4852:");
    LCD_WriteHex(rs485_result.port2_rx_count);
}

void InterfaceTest_Init(void)
{
    previous_key_1 = 0;
    previous_key_2 = 0;
    previous_key_3 = 0;
    screen_page = 0;
    screen_timer = 0;
    rs485_result.port1_rx_count = 0;
    rs485_result.port2_rx_count = 0;
    rs485_result.last_port = 0;
    rs485_result.last_byte = 0;

    LCD_SetCursor(0, 0);
    LCD_WriteText("PREPAID IF TEST ");
    LCD_SetCursor(1, 0);
    LCD_WriteText("MS51PC0AE        ");

    InterfaceTest_RunOutputs();
    I2C_RunReadOnlyTests(&i2c_result);

    LCD_Command(0x01);
    InterfaceTest_ShowStatus();
}

void InterfaceTest_Poll(void)
{
    uint8_t key_1;
    uint8_t key_2;
    uint8_t key_3;

    RS485_Poll(&rs485_result);
    key_1 = Board_KeyPressed(1);
    key_2 = Board_KeyPressed(2);
    key_3 = Board_KeyPressed(3);

    if (key_1 && !previous_key_1)
    {
        RS485_SendManualTest(1);
    }
    if (key_2 && !previous_key_2)
    {
        RS485_SendManualTest(2);
    }
    if (key_3 && !previous_key_3)
    {
        screen_page ^= 1;
        if (screen_page == 0)
        {
            I2C_RunReadOnlyTests(&i2c_result);
        }
    }

    previous_key_1 = key_1;
    previous_key_2 = key_2;
    previous_key_3 = key_3;

    if (screen_timer >= 500)
    {
        screen_timer = 0;
        if (screen_page == 0)
        {
            InterfaceTest_ShowStatus();
        }
        else
        {
            LCD_SetCursor(0, 0);
            LCD_WriteText("KEY1=485-1 TX   ");
            LCD_SetCursor(1, 0);
            LCD_WriteText("KEY2=485-2 TX   ");
        }
    }

    Board_DelayMs(1);
    screen_timer++;
}
