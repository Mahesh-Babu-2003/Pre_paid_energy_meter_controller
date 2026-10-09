#include "ms51_device.h"
#include "sys.h"
#include "board_io.h"
#include "i2c_probe.h"
#include "interface_test.h"
#include "lcd_595.h"
#include "rs485_test.h"

void main(void)
{
    MODIFY_HIRC(HIRC_24);
    SFRS = 0;
    Board_Init();
    I2C_TestInit();
    RS485_TestInit();
    LCD_Init();
    InterfaceTest_Init();

    while (1)
    {
        InterfaceTest_Poll();
    }
}
