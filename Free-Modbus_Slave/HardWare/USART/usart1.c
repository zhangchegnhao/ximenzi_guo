/************************************************************
 * 版权：2025CIMC Copyright。
 * 文件：usart.c
 * 作者: Jialei Zhao
 * 平台: 2025CIMC IHD-V04
 * 版本: Jialei Zhao     2025/12/30     V0.01    original
************************************************************/


/************************* 头文件 *************************/
#include "usart1.h"

/************************* 宏定义 *************************/



/************************ 变量定义 ************************/


/************************ 函数定义 ************************/
void usart_485_CS(uint8_t cs);

void my_usart1_init(void)
{
	nvic_irq_enable(USART1_IRQn , 5 , 0);
	rcu_periph_clock_enable(USARTX_RCU);
	rcu_periph_clock_enable(USART_PIN_RCU);
	rcu_periph_clock_enable(USART_485_CS_RCU);

	gpio_mode_set(USART_485_CS_PORT , GPIO_MODE_OUTPUT , GPIO_PUPD_PULLDOWN , USARTX_485_CS_Pin);
	gpio_output_options_set(USART_485_CS_PORT , GPIO_OTYPE_PP , GPIO_OSPEED_50MHZ , USARTX_485_CS_Pin);

	gpio_af_set(USART_PORT , GPIO_AF_7 , USART_TX_Pin);
	gpio_mode_set(USART_PORT , GPIO_MODE_AF , GPIO_PUPD_NONE , USART_TX_Pin);
	gpio_output_options_set(USART_PORT , GPIO_OTYPE_PP , GPIO_OSPEED_50MHZ , USART_TX_Pin);

	gpio_af_set(USART_PORT , GPIO_AF_7 , USART_RX_Pin);
	gpio_mode_set(USART_PORT , GPIO_MODE_AF , GPIO_PUPD_NONE , USART_RX_Pin);
	gpio_output_options_set(USART_PORT , GPIO_OTYPE_PP , GPIO_OSPEED_50MHZ , USART_RX_Pin);

	usart_deinit(USART);
	usart_baudrate_set(USART , 115200U);
	usart_transmit_config(USART , USART_TRANSMIT_ENABLE);
	usart_receive_config(USART , USART_RECEIVE_ENABLE);

	// usart_interrupt_enable(USART , USART_INT_RBNE);
	// usart_interrupt_enable(USART , USART_INT_IDLE);
	usart_enable(USART);
}


void my_usart_485_CS(uint8_t cs)
{
	if (cs == 1)
	{
		gpio_bit_set(USART_485_CS_PORT , USARTX_485_CS_Pin);
	}
	else
	{
		gpio_bit_reset(USART_485_CS_PORT , USARTX_485_CS_Pin);
	}
}


