/*!
    \file    gd32f4xx_it.c
    \brief   interrupt service routines

    \version 2024-12-20, V3.3.1, firmware for GD32F4xx
*/

/*
    Copyright (c) 2024, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32f4xx_it.h"
#include "main.h"
#include "systick.h"
#include "sdio_sdcard.h"
#include "mcu_cimc_gd32f470vet6.h"
#include "ota_uart.h"
#include "string.h"

extern uint8_t rxbuffer[USART_RXBUFFER_SIZE];
extern uint8_t rs_rxbuffer[AUX_USART_RXBUFFER_SIZE];
extern uint8_t ttl_rxbuffer[AUX_USART_RXBUFFER_SIZE];
extern uint8_t uart_dma_buffer[512];
extern uint8_t rx_flag;
extern volatile uint8_t  rs_rx_flag;
extern volatile uint16_t rs_rx_len;

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    /* if NMI exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    /* if Hard Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles MemManage exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void MemManage_Handler(void)
{
    /* if Memory Manage exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles BusFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void BusFault_Handler(void)
{
    /* if Bus Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles UsageFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void UsageFault_Handler(void)
{
    /* if Usage Fault exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
    /* if SVC exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles DebugMon exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void DebugMon_Handler(void)
{
    /* if DebugMon exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
    /* if PendSV exception occurs, go to infinite loop */
    while(1) {
    }
}

/*!
    \brief      this function handles USART interrupt request
    \param[in]  none
    \param[out] none
    \retval     none
*/
void USART0_IRQHandler(void)
{
    uint32_t rx_len;
    uint32_t copy_len;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE)){
        /* clear IDLE flag */
        usart_data_receive(USART0);
        dma_channel_disable(DMA_DEBUG_USART, DMA_DEBUG_USART_CHANNEL_RX);
        
        /* number of data received */
        rx_len = sizeof(rxbuffer) - dma_transfer_number_get(DMA_DEBUG_USART, DMA_DEBUG_USART_CHANNEL_RX);
        if((rx_len > 0U) && (rx_len <= sizeof(rxbuffer))){
            copy_len = rx_len;
            if(copy_len >= sizeof(uart_dma_buffer)){
                copy_len = sizeof(uart_dma_buffer) - 1U;
            }
            memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
            memcpy(uart_dma_buffer, rxbuffer, copy_len);
            rx_flag = 1;
        }
        memset(rxbuffer, 0, sizeof(rxbuffer));
        
        /* disable DMA and reconfigure */
        dma_flag_clear(DMA_DEBUG_USART, DMA_DEBUG_USART_CHANNEL_RX, DMA_FLAG_FTF);
        dma_transfer_number_config(DMA_DEBUG_USART, DMA_DEBUG_USART_CHANNEL_RX, sizeof(rxbuffer));
        dma_channel_enable(DMA_DEBUG_USART, DMA_DEBUG_USART_CHANNEL_RX);
    }
}

void USART1_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(RS232_RS485_USART, USART_INT_FLAG_IDLE)){
        usart_data_receive(RS232_RS485_USART);
        dma_channel_disable(DMA_RS_USART, DMA_RS_USART_CHANNEL_RX);

        uint32_t len = sizeof(rs_rxbuffer) - dma_transfer_number_get(DMA_RS_USART, DMA_RS_USART_CHANNEL_RX);
        if(len > 0U){
            /* OTA 接收：把本次 RS485 收到的全部字节复制到 OTA ringbuffer，
               由 ota_uart_task() 在主循环中识别 OTA 头并写下载缓存。
               常规协议帧解析仍走 rs_rx_flag 路径，二者并行。 */
            ota_uart_process_frame(rs_rxbuffer, len);

            rs_rx_len  = (uint16_t)len;
            rs_rx_flag = 1U;
        }

        dma_flag_clear(DMA_RS_USART, DMA_RS_USART_CHANNEL_RX, DMA_FLAG_FTF);
        dma_transfer_number_config(DMA_RS_USART, DMA_RS_USART_CHANNEL_RX, sizeof(rs_rxbuffer));
        dma_channel_enable(DMA_RS_USART, DMA_RS_USART_CHANNEL_RX);
    }
}

void USART2_IRQHandler(void)
{
    if(RESET != usart_interrupt_flag_get(TTL_USART, USART_INT_FLAG_IDLE)){
        usart_data_receive(TTL_USART);
        dma_channel_disable(DMA_TTL_USART, DMA_TTL_USART_CHANNEL_RX);
        dma_flag_clear(DMA_TTL_USART, DMA_TTL_USART_CHANNEL_RX, DMA_FLAG_FTF);
        dma_transfer_number_config(DMA_TTL_USART, DMA_TTL_USART_CHANNEL_RX, sizeof(ttl_rxbuffer));
        dma_channel_enable(DMA_TTL_USART, DMA_TTL_USART_CHANNEL_RX);
    }
}

void EXTI0_IRQHandler(void)
{
    if(RESET != exti_interrupt_flag_get(EXTI_0)) {
        exti_interrupt_flag_clear(EXTI_0);
    }
}

void SDIO_IRQHandler(void)
{
    sd_interrupts_process();
}

/*!
    \brief    this function handles SysTick exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SysTick_Handler(void)
{
    delay_decrement();
}
