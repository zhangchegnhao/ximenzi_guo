/************************************************************
 * 版权：2025CIMC Copyright。
 * 文件：usart.h
 * 作者: Jialei Zhao
 * 平台: 2025CIMC IHD-V04
 * 版本: Jialei Zhao     2025/12/30     V0.01    original
************************************************************/

#ifndef __USART_H__
#define __USART_H__

/************************* 头文件 *************************/
#include "HeaderFiles.h"

/************************* 宏定义 *************************/
#define USART_PORT GPIOA
#define USART USART1
#define USART_TX_Pin GPIO_PIN_2
#define USART_RX_Pin GPIO_PIN_3
#define USARTX_RCU RCU_USART1
#define USART_PIN_RCU RCU_GPIOA


#define USART_485_CS_RCU RCU_GPIOA
#define USART_485_CS_PORT GPIOA
#define USARTX_485_CS_Pin GPIO_PIN_1

#define USARTX_485_Send 1
#define USARTX_485_Receive 0



/************************ 变量定义 ************************/

/************************ 函数定义 ************************/

void my_usart1_init(void);

void my_usart_485_CS(uint8_t cs);


#endif // !__USART_H__
/****************************End*****************************/
