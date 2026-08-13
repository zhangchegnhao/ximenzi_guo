/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#ifndef MCU_CIMC_GD32F470VET6_H
#define MCU_CIMC_GD32F470VET6_H

#include "gd32f4xx.h"
#include "systick.h"

#include "usart_app.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************************/

/* RS485 debug UART: USART1 TX/RX = PA2/PA3, direction = PA1. */
#define RS485_CS_PORT             GPIOA
#define RS485_CS_PORT_RCU         RCU_GPIOA
#define RS485_CS_PIN              GPIO_PIN_1
#define RS485_CS_SET(x)           do { if(x) GPIO_BOP(RS485_CS_PORT) = RS485_CS_PIN; else GPIO_BC(RS485_CS_PORT) = RS485_CS_PIN; } while(0)

#define DEBUG_USART               USART1
#define DEBUG_USART_RCU           RCU_USART1
#define DEBUG_USART_AF            GPIO_AF_7
#define DEBUG_USART_PORT          GPIOA
#define DEBUG_USART_PORT_RCU      RCU_GPIOA
#define DEBUG_USART_TX_PIN        GPIO_PIN_2
#define DEBUG_USART_RX_PIN        GPIO_PIN_3

/* OLED I2C0：与 APP 侧引脚一致 PB8/PB9，方便 APP→BL→APP 显存连贯保留 */
#define I2C0_OWN_ADDRESS7      0x72
#define I2C0_DATA_ADDRESS      (uint32_t)&I2C_DATA(I2C0)
#define OLED_PORT              GPIOB
#define OLED_CLK_PORT_RCU      RCU_GPIOB
#define OLED_DAT_PIN           GPIO_PIN_9
#define OLED_CLK_PIN           GPIO_PIN_8

// FUNCTION
void bsp_usart_init(void);
void bsp_oled_init(void);

/***************************************************************************************************************/

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CIMC_GD32F470VET6_H */
