/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

__IO uint16_t tx_count = 0;

int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;
    /* Start variable argument formatting. */
    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if(usart_periph == DEBUG_USART) {
        RS485_CS_SET(1);
    }

    for(tx_count = 0; tx_count < len; tx_count++){
        usart_data_transmit(usart_periph, buffer[tx_count]);
        while(RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    }

    if(usart_periph == DEBUG_USART) {
        while(RESET == usart_flag_get(usart_periph, USART_FLAG_TC));
        RS485_CS_SET(0);
    }
    
    return len;
}
