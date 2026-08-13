/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "gd32f4xx_i2c.h"
#include "gd32f4xx_dma.h"

/* OLED control/data buffers - 与 APP 侧字节序一致，名字 / 值都不变。
   oled.c 通过 extern 引用，所以 BL 与 APP 共用同一份 oled.c 时也能链上。 */
__IO uint8_t oled_cmd_buf[2]  = {0x00, 0x00};  /* control byte + command */
__IO uint8_t oled_data_buf[2] = {0x40, 0x00};  /* control byte + data   */

/*!
    \brief      configure USART
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_usart_init(void)
{
    /* enable GPIO clock */
    rcu_periph_clock_enable(DEBUG_USART_PORT_RCU);
    rcu_periph_clock_enable(RS485_CS_PORT_RCU);

    /* enable USART clock */
    rcu_periph_clock_enable(DEBUG_USART_RCU);
    
    /* connect port to USARTx_Tx */
    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_TX_PIN);

    /* connect port to USARTx_Rx */
    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_RX_PIN);

    /* configure USART Tx as alternate function push-pull */
    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_TX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_TX_PIN);

    /* configure USART Rx as alternate function push-pull */
    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_RX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_RX_PIN);

    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);

    /* configure USART
       N-01 / OTA：BL 与 APP 复用同一条 RS485 总线，波特率必须一致。
       APP 默认 19200，BL 不能持久化波特率，固定 19200。 */
    usart_deinit(DEBUG_USART);
    usart_baudrate_set(DEBUG_USART, 19200U);
    usart_receive_config(DEBUG_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(DEBUG_USART, USART_TRANSMIT_ENABLE);
    usart_enable(DEBUG_USART);
}

/* I2C0 + DMA0_CH6 + PB8/PB9 初始化，与 APP 侧 bsp_oled_init 一致。
   逐行抄自 app/Driver/Components/bsp/mcu_cimc_gd32f470vet6.c。 */
void bsp_oled_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(OLED_CLK_PORT_RCU);
    rcu_periph_clock_enable(RCU_I2C0);
    rcu_periph_clock_enable(RCU_DMA0);

    gpio_af_set(OLED_PORT, GPIO_AF_4, OLED_DAT_PIN);
    gpio_af_set(OLED_PORT, GPIO_AF_4, OLED_CLK_PIN);

    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_DAT_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_DAT_PIN);
    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_CLK_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_CLK_PIN);

    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C0_OWN_ADDRESS7);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

    dma_deinit(DMA0, DMA_CH6);
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr        = (uint32_t)oled_data_buf;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number              = 2;
    dma_init_struct.periph_addr         = I2C0_DATA_ADDRESS;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority            = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH6, &dma_init_struct);

    dma_circulation_disable(DMA0, DMA_CH6);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI1);
}
