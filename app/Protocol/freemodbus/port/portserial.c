#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "mcu_cimc_gd32f470vet6.h"

BOOL xMBPortSerialInit(UCHAR port, ULONG baudrate, UCHAR data_bits,
                       eMBParity parity)
{
    (void)port;

    if (data_bits != 8U) {
        return FALSE;
    }

    bsp_rs_usart_init();
    usart_disable(RS232_RS485_USART);
    usart_baudrate_set(RS232_RS485_USART, baudrate);
    usart_word_length_set(RS232_RS485_USART, USART_WL_8BIT);
    usart_stop_bit_set(RS232_RS485_USART, USART_STB_1BIT);

    switch (parity) {
    case MB_PAR_NONE:
        usart_parity_config(RS232_RS485_USART, USART_PM_NONE);
        break;
    case MB_PAR_EVEN:
        usart_parity_config(RS232_RS485_USART, USART_PM_EVEN);
        break;
    case MB_PAR_ODD:
        usart_parity_config(RS232_RS485_USART, USART_PM_ODD);
        break;
    default:
        return FALSE;
    }

    usart_enable(RS232_RS485_USART);
    RS485_CS_SET(0);
    return TRUE;
}

void vMBPortSerialEnable(BOOL rx_enable, BOOL tx_enable)
{
    if (rx_enable == TRUE) {
        usart_interrupt_enable(RS232_RS485_USART, USART_INT_RBNE);
    } else {
        usart_interrupt_disable(RS232_RS485_USART, USART_INT_RBNE);
    }

    if (tx_enable == TRUE) {
        RS485_CS_SET(1);
        usart_interrupt_enable(RS232_RS485_USART, USART_INT_TBE);
    } else {
        usart_interrupt_disable(RS232_RS485_USART, USART_INT_TBE);
        while (usart_flag_get(RS232_RS485_USART, USART_FLAG_TC) == RESET) {
            __NOP();
        }
        RS485_CS_SET(0);
    }
}

BOOL xMBPortSerialPutByte(CHAR byte)
{
    usart_data_transmit(RS232_RS485_USART, (uint8_t)byte);
    return TRUE;
}

BOOL xMBPortSerialGetByte(CHAR *byte)
{
    if (byte == NULL) {
        return FALSE;
    }
    *byte = (CHAR)usart_data_receive(RS232_RS485_USART);
    return TRUE;
}

void mb_port_serial_irq_handler(void)
{
    if (usart_interrupt_flag_get(RS232_RS485_USART,
                                 USART_INT_FLAG_RBNE) != RESET) {
        (void)pxMBFrameCBByteReceived();
    }

    if (usart_interrupt_flag_get(RS232_RS485_USART,
                                 USART_INT_FLAG_TBE) != RESET) {
        (void)pxMBFrameCBTransmitterEmpty();
    }
}
