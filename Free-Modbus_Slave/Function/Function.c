#include "Function.h"
#include "mb.h"
#include "port.h"

#define MODBUS_SLAVE_ADDRESS  1U
#define MODBUS_BAUDRATE       115200UL

static const uint16_t s_input_registers[6] = {
    0x1234U,
    0x5678U,
    0x0001U,
    0x0002U,
    0x0003U,
    0x0004U
};

void System_Init(void)
{
    eMBErrorCode status;

    nvic_priority_group_set(NVIC_PRIGROUP_PRE4_SUB0);
    systick_config();

    Set_Register(REG_INPUT, 0U, s_input_registers,
                 (uint16_t)(sizeof(s_input_registers) / sizeof(s_input_registers[0])));

    status = eMBInit(MB_RTU, MODBUS_SLAVE_ADDRESS, 0U,
                     MODBUS_BAUDRATE, MB_PAR_NONE);
    if (status == MB_ENOERR) {
        status = eMBEnable();
    }

    if (status != MB_ENOERR) {
        while (1) {
        }
    }
}

void UsrFunction(void)
{
    while (1) {
        (void)eMBPoll();
    }
}

void __aeabi_assert(const char *expr, const char *file, int line)
{
    (void)expr;
    (void)file;
    (void)line;

    __disable_irq();
    while (1) {
    }
}
