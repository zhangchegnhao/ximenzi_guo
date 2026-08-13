/*!
    \file    gd30ad3344.h
    \brief   definitions for the gd30ad3344

    \version 2026-5-1, V1.0.0, firmware for GD30AD3344
*/

#ifndef __GD30AD3344__H
#define __GD30AD3344__H

#ifdef __cplusplus
extern "C" {
#endif

#include "mcu_cimc_gd32f470vet6.h"

typedef struct _GD30AD3344
{
    uint16_t SS         : 1;
    uint16_t MUX        : 3;
    uint16_t PGA        : 3;
    uint16_t MODE       : 1;
    uint16_t DR         : 3;
    uint16_t RESERVED_1 : 1;
    uint16_t PULL_UP_EN : 1;
    uint16_t NOP        : 2;
    uint16_t RESERVED   : 1;
}GD30AD3344;

extern GD30AD3344 GD30AD3344_InitStruct;

typedef enum
{
    GD30AD3344_OS_DISABLE            = 0,
    GD30AD3344_OS_SINGLE_CONVERT     = 1,
} GD30AD3344_OS_TypeDef;

typedef enum
{
    GD30AD3344_Channel_0             = 0, /* AINP=AIN0, AINN=AIN1 */
    GD30AD3344_Channel_1             = 1, /* AINP=AIN0, AINN=AIN3 */
    GD30AD3344_Channel_2             = 2, /* AINP=AIN1, AINN=AIN3 */
    GD30AD3344_Channel_3             = 3, /* AINP=AIN2, AINN=AIN3 */
    GD30AD3344_Channel_4             = 4, /* AINP=AIN0, AINN=GND  */
    GD30AD3344_Channel_5             = 5, /* AINP=AIN1, AINN=GND  */
    GD30AD3344_Channel_6             = 6, /* AINP=AIN2, AINN=GND  */
    GD30AD3344_Channel_7             = 7, /* AINP=AIN3, AINN=GND  */
} GD30AD3344_Channel_TypeDef;

#define GD30AD3344_MUX_AIN0_AIN1     GD30AD3344_Channel_0
#define GD30AD3344_MUX_AIN0_AIN3     GD30AD3344_Channel_1
#define GD30AD3344_MUX_AIN1_AIN3     GD30AD3344_Channel_2
#define GD30AD3344_MUX_AIN2_AIN3     GD30AD3344_Channel_3
#define GD30AD3344_MUX_AIN0_GND      GD30AD3344_Channel_4
#define GD30AD3344_MUX_AIN1_GND      GD30AD3344_Channel_5
#define GD30AD3344_MUX_AIN2_GND      GD30AD3344_Channel_6
#define GD30AD3344_MUX_AIN3_GND      GD30AD3344_Channel_7

typedef enum
{
    GD30AD3344_PGA_6V144             = 0,
    GD30AD3344_PGA_4V096             = 1,
    GD30AD3344_PGA_2V048             = 2,
    GD30AD3344_PGA_1V024             = 3,
    GD30AD3344_PGA_0V512             = 4,
    GD30AD3344_PGA_0V256             = 5,
    GD30AD3344_PGA_0V064             = 6,
    GD30AD3344_PGA_RESERVED          = 7,
} GD30AD3344_PGA_TypeDef;

typedef enum
{
    GD30AD3344_MODE_CONTINUOUS       = 0,
    GD30AD3344_MODE_SINGLE_SHOT      = 1,
} GD30AD3344_Mode_TypeDef;

typedef enum
{
    GD30AD3344_DR_6_25SPS            = 0,
    GD30AD3344_DR_12_5SPS            = 1,
    GD30AD3344_DR_25SPS              = 2,
    GD30AD3344_DR_50SPS              = 3,
    GD30AD3344_DR_100SPS             = 4,
    GD30AD3344_DR_250SPS             = 5,
    GD30AD3344_DR_500SPS             = 6,
    GD30AD3344_DR_1000SPS            = 7,
} GD30AD3344_DR_TypeDef;

typedef enum
{
    GD30AD3344_PULL_UP_DISABLE       = 0,
    GD30AD3344_PULL_UP_ENABLE        = 1,
} GD30AD3344_PullUp_TypeDef;

typedef enum
{
    GD30AD3344_NOP_INVALID_NO_UPDATE = 0,
    GD30AD3344_NOP_VALID_UPDATE      = 1,
    GD30AD3344_NOP_VALID_NO_UPDATE   = 2,
    GD30AD3344_NOP_INVALID_NO_CHANGE = 3,
} GD30AD3344_NOP_TypeDef;

#define GD30AD3344_RESERVED_0        0U
#define GD30AD3344_RESERVED_1        1U

#define GD30AD3344_InitStruct_Value ((uint16_t)((GD30AD3344_InitStruct.SS         <<15)|\
                                                (GD30AD3344_InitStruct.MUX        <<12)|\
                                                (GD30AD3344_InitStruct.PGA        << 9)|\
                                                (GD30AD3344_InitStruct.MODE       << 8)|\
                                                (GD30AD3344_InitStruct.DR         << 5)|\
                                                (GD30AD3344_InitStruct.RESERVED_1 << 4)|\
                                                (GD30AD3344_InitStruct.PULL_UP_EN << 3)|\
                                                (GD30AD3344_InitStruct.NOP        << 1)|\
                                                (GD30AD3344_InitStruct.RESERVED   << 0)))

extern GD30AD3344 GD30AD3344_InitStruct;

void GD30AD3344_Init(void);

float GD30AD3344_AD_Read(GD30AD3344_Channel_TypeDef CH,GD30AD3344_PGA_TypeDef Ref);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__GD30AD3344__H
