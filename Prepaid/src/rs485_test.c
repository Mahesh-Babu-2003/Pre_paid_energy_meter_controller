#include "rs485_test.h"
#include "board_io.h"
#include "uart.h"

#define METER_SLAVE_ID          5
#define CONTROLLER_SLAVE_ID     1
#define METER_BAUD_RATE         9600UL
#define METER_REGISTER_START    7
#define METER_REGISTER_COUNT    10
#define RELAY_REGISTER_COUNT    3
#define SLAVE_REGISTER_COUNT    (RELAY_REGISTER_COUNT + METER_REGISTER_COUNT)
#define RTU_FRAME_GAP_MS        4
#define MASTER_TIMEOUT_MS       100
#define MASTER_POLL_INTERVAL_MS 250
#define RX_BUFFER_SIZE          48

static uint8_t xdata port1_rx_buffer[RX_BUFFER_SIZE];
static uint8_t xdata port2_rx_buffer[RX_BUFFER_SIZE];
static uint8_t xdata tx_buffer[RX_BUFFER_SIZE];
static uint16_t xdata meter_registers[METER_REGISTER_COUNT];
static uint8_t relay_states[RELAY_REGISTER_COUNT];
static volatile uint8_t port1_rx_length;
static volatile uint8_t port2_rx_length;
static volatile uint8_t port1_silence_ms;
static volatile uint8_t port2_silence_ms;
static volatile uint8_t port1_rx_parity_error;
static volatile uint8_t port2_rx_parity_error;
static volatile uint8_t port1_rx_count;
static volatile uint8_t port2_rx_count;
static volatile uint8_t port1_tx_complete;
static volatile uint8_t port2_tx_complete;
static uint8_t master_waiting;
static uint16_t master_timer_ms;
static uint16_t master_timeout_ms;
static uint8_t meter_data_valid;

static uint8_t RS485_EvenParity(uint8_t value)
{
    uint8_t parity;

    parity = 0;
    while (value != 0)
    {
        parity ^= (value & 1);
        value >>= 1;
    }
    return parity;
}

static void RS485_SendByte(uint8_t port, uint8_t value)
{
    uint8_t parity;

    parity = RS485_EvenParity(value);
    SFRS = 0;
    if (port == 1)
    {
        port1_tx_complete = 0;
        TB8_1 = parity;
        SBUF_1 = value;
        while (port1_tx_complete == 0) { }
    }
    else
    {
        port2_tx_complete = 0;
        TB8 = parity;
        SBUF = value;
        while (port2_tx_complete == 0) { }
    }
}

static uint16_t Modbus_Crc16(uint8_t xdata *buffer, uint8_t length)
{
    uint8_t index;
    uint8_t bit_index;
    uint16_t crc;

    crc = 0xFFFF;
    for (index = 0; index < length; index++)
    {
        crc ^= buffer[index];
        for (bit_index = 0; bit_index < 8; bit_index++)
        {
            if (crc & 1)
            {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static uint8_t Modbus_FrameValid(uint8_t xdata *frame, uint8_t length)
{
    uint16_t crc;

    if (length < 4)
    {
        return 0;
    }
    crc = Modbus_Crc16(frame, length - 2);
    return (frame[length - 2] == (uint8_t)crc &&
            frame[length - 1] == (uint8_t)(crc >> 8));
}

static void RS485_ClearMeterRegisters(void)
{
    uint8_t index;

    for (index = 0; index < METER_REGISTER_COUNT; index++)
    {
        meter_registers[index] = 0;
    }
}

void RS485_UART0_ISR(void) interrupt 4
{
    uint8_t sfr_page;
    uint8_t value;

    sfr_page = SFRS;
    SFRS = 0;
    if (RI)
    {
        value = SBUF;
        RI = 0;
        if (RB8 != RS485_EvenParity(value))
        {
            port2_rx_parity_error = 1;
        }
        if (port2_rx_length < RX_BUFFER_SIZE)
        {
            port2_rx_buffer[port2_rx_length++] = value;
            port2_silence_ms = 0;
        }
        if (port2_rx_count != 0xFF)
        {
            port2_rx_count++;
        }
    }
    if (TI)
    {
        TI = 0;
        port2_tx_complete = 1;
    }
    SFRS = sfr_page;
}

void RS485_UART1_ISR(void) interrupt 15
{
    uint8_t sfr_page;
    uint8_t value;

    sfr_page = SFRS;
    SFRS = 0;
    if (RI_1)
    {
        value = SBUF_1;
        RI_1 = 0;
        if (RB8_1 != RS485_EvenParity(value))
        {
            port1_rx_parity_error = 1;
        }
        if (port1_rx_length < RX_BUFFER_SIZE)
        {
            port1_rx_buffer[port1_rx_length++] = value;
            port1_silence_ms = 0;
        }
        if (port1_rx_count != 0xFF)
        {
            port1_rx_count++;
        }
    }
    if (TI_1)
    {
        TI_1 = 0;
        port1_tx_complete = 1;
    }
    SFRS = sfr_page;
}

static void RS485_SendFrame(uint8_t port, uint8_t xdata *frame, uint8_t length)
{
    uint8_t index;
    uint16_t crc;

    crc = Modbus_Crc16(frame, length);
    frame[length] = (uint8_t)crc;
    frame[length + 1] = (uint8_t)(crc >> 8);

    Board_SetRs485Transmit(port, 1);
    if (port == 1)
    {
        Board_DelayMs(1);
    }
    for (index = 0; index < (uint8_t)(length + 2); index++)
    {
        RS485_SendByte(port, frame[index]);
    }
    Board_DelayMs(2);
    Board_SetRs485Transmit(port, 0);
}

static void RS485_SetRelay(uint8_t index, uint16_t value)
{
    if (index >= RELAY_REGISTER_COUNT || value > 1)
    {
        return;
    }
    if (relay_states[index] != (uint8_t)value)
    {
        Board_PulseRelay((uint8_t)(index + 1), (uint8_t)value, 120);
        relay_states[index] = (uint8_t)value;
    }
}

static void Modbus_SendException(uint8_t function, uint8_t exception_code)
{
    tx_buffer[0] = CONTROLLER_SLAVE_ID;
    tx_buffer[1] = function | 0x80;
    tx_buffer[2] = exception_code;
    RS485_SendFrame(2, tx_buffer, 3);
}

static void Modbus_HandleSlaveFrame(uint8_t length)
{
    uint8_t function;
    uint8_t quantity;
    uint8_t index;
    uint8_t broadcast;
    uint16_t start_address;
    uint16_t value;
    uint16_t write_count;

    if (!Modbus_FrameValid(port2_rx_buffer, length))
    {
        return;
    }
    if (port2_rx_buffer[0] != CONTROLLER_SLAVE_ID &&
        port2_rx_buffer[0] != 0)
    {
        return;
    }

    broadcast = (port2_rx_buffer[0] == 0);
    function = port2_rx_buffer[1];
    start_address = ((uint16_t)port2_rx_buffer[2] << 8) |
                    port2_rx_buffer[3];

    if (function == 0x03)
    {
        if (length != 8)
        {
            return;
        }
        if (port2_rx_buffer[4] != 0)
        {
            if (!broadcast)
            {
                Modbus_SendException(function, 0x03);
            }
            return;
        }
        quantity = port2_rx_buffer[5];
        if (broadcast)
        {
            return;
        }
        if (quantity == 0 || quantity > SLAVE_REGISTER_COUNT ||
            start_address >= SLAVE_REGISTER_COUNT ||
            (uint16_t)(start_address + quantity) > SLAVE_REGISTER_COUNT)
        {
            Modbus_SendException(function, 0x02);
            return;
        }
        tx_buffer[0] = CONTROLLER_SLAVE_ID;
        tx_buffer[1] = function;
        tx_buffer[2] = quantity * 2;
        for (index = 0; index < quantity; index++)
        {
            if ((start_address + index) < RELAY_REGISTER_COUNT)
            {
                value = relay_states[start_address + index];
            }
            else
            {
                value = meter_registers[
                    start_address + index - RELAY_REGISTER_COUNT];
            }
            tx_buffer[3 + (index * 2)] = (uint8_t)(value >> 8);
            tx_buffer[4 + (index * 2)] = (uint8_t)value;
        }
        RS485_SendFrame(2, tx_buffer, (uint8_t)(3 + quantity * 2));
        return;
    }

    if (function == 0x06)
    {
        if (length != 8)
        {
            return;
        }
        value = ((uint16_t)port2_rx_buffer[4] << 8) | port2_rx_buffer[5];
        if (start_address >= RELAY_REGISTER_COUNT)
        {
            if (!broadcast)
            {
                Modbus_SendException(function, 0x02);
            }
            return;
        }
        if (value > 1)
        {
            if (!broadcast)
            {
                Modbus_SendException(function, 0x03);
            }
            return;
        }
        RS485_SetRelay((uint8_t)start_address, value);
        if (!broadcast)
        {
            RS485_SendFrame(2, port2_rx_buffer, 6);
        }
        return;
    }

    if (function == 0x10)
    {
        if (length < 9)
        {
            return;
        }
        write_count = ((uint16_t)port2_rx_buffer[4] << 8) |
                      port2_rx_buffer[5];
        quantity = port2_rx_buffer[6];
        if (write_count == 0 || write_count > RELAY_REGISTER_COUNT ||
            quantity != write_count * 2 ||
            length != (uint8_t)(9 + quantity) ||
            start_address >= RELAY_REGISTER_COUNT ||
            (uint16_t)(start_address + write_count) > RELAY_REGISTER_COUNT)
        {
            if (!broadcast)
            {
                Modbus_SendException(function, 0x02);
            }
            return;
        }
        for (index = 0; index < (uint8_t)write_count; index++)
        {
            value = ((uint16_t)port2_rx_buffer[7 + (index * 2)] << 8) |
                    port2_rx_buffer[8 + (index * 2)];
            if (value > 1)
            {
                if (!broadcast)
                {
                    Modbus_SendException(function, 0x03);
                }
                return;
            }
        }
        for (index = 0; index < (uint8_t)write_count; index++)
        {
            value = ((uint16_t)port2_rx_buffer[7 + (index * 2)] << 8) |
                    port2_rx_buffer[8 + (index * 2)];
            RS485_SetRelay((uint8_t)(start_address + index), value);
        }
        if (!broadcast)
        {
            tx_buffer[0] = CONTROLLER_SLAVE_ID;
            tx_buffer[1] = function;
            tx_buffer[2] = (uint8_t)(start_address >> 8);
            tx_buffer[3] = (uint8_t)start_address;
            tx_buffer[4] = (uint8_t)(write_count >> 8);
            tx_buffer[5] = (uint8_t)write_count;
            RS485_SendFrame(2, tx_buffer, 6);
        }
        return;
    }

    if (!broadcast)
    {
        Modbus_SendException(function, 0x01);
    }
}

static void RS485_StartMeterRead(void)
{
    tx_buffer[0] = METER_SLAVE_ID;
    tx_buffer[1] = 0x03;
    tx_buffer[2] = (uint8_t)(METER_REGISTER_START >> 8);
    tx_buffer[3] = (uint8_t)METER_REGISTER_START;
    tx_buffer[4] = 0;
    tx_buffer[5] = METER_REGISTER_COUNT;
    port1_rx_length = 0;
    port1_rx_parity_error = 0;
    port1_silence_ms = 0;
    RS485_SendFrame(1, tx_buffer, 6);
    master_waiting = 1;
    master_timeout_ms = 0;
}

static void RS485_HandleMeterResponse(uint8_t length)
{
    uint8_t index;
    uint8_t expected_length;

    if (!master_waiting ||
        !Modbus_FrameValid(port1_rx_buffer, length) ||
        port1_rx_buffer[0] != METER_SLAVE_ID)
    {
        return;
    }
    if (port1_rx_buffer[1] & 0x80)
    {
        meter_data_valid = 0;
        RS485_ClearMeterRegisters();
        master_waiting = 0;
        return;
    }
    expected_length = (uint8_t)(5 + METER_REGISTER_COUNT * 2);
    if (port1_rx_buffer[1] != 0x03 ||
        port1_rx_buffer[2] != (METER_REGISTER_COUNT * 2) ||
        length != expected_length)
    {
        meter_data_valid = 0;
        RS485_ClearMeterRegisters();
        master_waiting = 0;
        return;
    }
    for (index = 0; index < METER_REGISTER_COUNT; index++)
    {
        meter_registers[index] =
            ((uint16_t)port1_rx_buffer[3 + (index * 2)] << 8) |
            port1_rx_buffer[4 + (index * 2)];
    }
    meter_data_valid = 1;
    master_waiting = 0;
}

void RS485_TestInit(void)
{
    uint8_t index;

    IE &= 0x7F;
    P37_INPUT_MODE;
    P36_QUASI_MODE;
    P06_PUSHPULL_MODE;
    P07_INPUT_MODE;
    ENABLE_UART1_TXD_P36;
    ENABLE_UART1_RXD_P37;
    SFRS = 0;
    UART_Open(24000000UL, UART1_Timer3, METER_BAUD_RATE);
    UART_Open(24000000UL, UART0_Timer1, METER_BAUD_RATE);

    /* Mode 3 sends 8 data bits plus a manually generated even-parity bit. */
    SCON_1 = 0xD0;
    SCON = 0xD0;
    TI_1 = 0;
    TI = 0;
    RI_1 = 0;
    RI = 0;
    port1_rx_count = 0;
    port2_rx_count = 0;
    port1_tx_complete = 0;
    port2_tx_complete = 0;
    Board_SetRs485Transmit(1, 0);
    Board_SetRs485Transmit(2, 0);
    Board_SetLed(1, 0);
    Board_SetLed(2, 0);

    for (index = 0; index < METER_REGISTER_COUNT; index++)
    {
        meter_registers[index] = 0;
    }
    for (index = 0; index < RELAY_REGISTER_COUNT; index++)
    {
        relay_states[index] = 0;
        Board_PulseRelay((uint8_t)(index + 1), 0, 120);
    }
    port1_rx_length = 0;
    port2_rx_length = 0;
    port1_silence_ms = 0;
    port2_silence_ms = 0;
    port1_rx_parity_error = 0;
    port2_rx_parity_error = 0;
    master_waiting = 0;
    master_timer_ms = MASTER_POLL_INTERVAL_MS;
    master_timeout_ms = 0;
    meter_data_valid = 0;
    EA = 1;
}

void RS485_Poll(RS485_TestResult *result)
{
    uint8_t frame_length;
    uint8_t parity_error;
    uint8_t global_interrupt_enabled;

    SFRS = 0;
    if (port1_rx_length != 0 && port1_silence_ms >= RTU_FRAME_GAP_MS)
    {
        global_interrupt_enabled = EA;
        EA = 0;
        frame_length = port1_rx_length;
        parity_error = port1_rx_parity_error;
        port1_rx_length = 0;
        port1_rx_parity_error = 0;
        EA = global_interrupt_enabled;
        if (!parity_error)
        {
            RS485_HandleMeterResponse(frame_length);
        }
    }
    if (port2_rx_length != 0 && port2_silence_ms >= RTU_FRAME_GAP_MS)
    {
        global_interrupt_enabled = EA;
        EA = 0;
        frame_length = port2_rx_length;
        parity_error = port2_rx_parity_error;
        port2_rx_length = 0;
        port2_rx_parity_error = 0;
        EA = global_interrupt_enabled;
        if (!parity_error)
        {
            Modbus_HandleSlaveFrame(frame_length);
        }
    }

    if (master_waiting)
    {
        if (master_timeout_ms < MASTER_TIMEOUT_MS)
        {
            master_timeout_ms++;
        }
        else
        {
            master_waiting = 0;
            port1_rx_length = 0;
            port1_rx_parity_error = 0;
            meter_data_valid = 0;
            RS485_ClearMeterRegisters();
        }
    }
    else if (master_timer_ms < MASTER_POLL_INTERVAL_MS)
    {
        master_timer_ms++;
    }
    else
    {
        master_timer_ms = 0;
        RS485_StartMeterRead();
    }

    if (port1_silence_ms < RTU_FRAME_GAP_MS)
    {
        port1_silence_ms++;
    }
    if (port2_silence_ms < RTU_FRAME_GAP_MS)
    {
        port2_silence_ms++;
    }

    result->last_port = 0;
    result->last_byte = 0;
    result->port1_rx_count = port1_rx_count;
    result->port2_rx_count = port2_rx_count;
}

uint16_t RS485_GetMeterRegister(uint8_t zero_based_address)
{
    if (zero_based_address >= METER_REGISTER_COUNT)
    {
        return 0;
    }
    return meter_registers[zero_based_address];
}

uint8_t RS485_IsMeterDataValid(void)
{
    return meter_data_valid;
}

uint8_t RS485_GetRelayState(uint8_t relay)
{
    if (relay == 0 || relay > RELAY_REGISTER_COUNT)
    {
        return 0;
    }
    return relay_states[relay - 1];
}
