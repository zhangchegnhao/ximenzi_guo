#include "mb_registers.h"
#include "mb.h"
#include <stddef.h>

static uint16_t s_input_registers[MB_INPUT_REGISTER_COUNT];

void mb_registers_init(void)
{
    static const uint16_t baseline[MB_INPUT_REGISTER_COUNT] = {
        0x1234U, 0x5678U, 0x0001U, 0x0002U, 0x0003U, 0x0004U
    };
    uint16_t i;

    for (i = 0U; i < MB_INPUT_REGISTER_COUNT; ++i) {
        s_input_registers[i] = baseline[i];
    }
}

eMBErrorCode eMBRegInputCB(UCHAR *buffer, USHORT address, USHORT count)
{
    USHORT index;

    if (buffer == NULL || address == 0U || count == 0U) {
        return MB_EINVAL;
    }

    index = (USHORT)(address - 1U);
    if (index >= MB_INPUT_REGISTER_COUNT ||
        count > (USHORT)(MB_INPUT_REGISTER_COUNT - index)) {
        return MB_ENOREG;
    }

    while (count > 0U) {
        *buffer++ = (UCHAR)(s_input_registers[index] >> 8U);
        *buffer++ = (UCHAR)(s_input_registers[index] & 0xFFU);
        ++index;
        --count;
    }

    return MB_ENOERR;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *buffer, USHORT address, USHORT count,
                              eMBRegisterMode mode)
{
    (void)buffer;
    (void)address;
    (void)count;
    (void)mode;
    return MB_ENOREG;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *buffer, USHORT address, USHORT count,
                            eMBRegisterMode mode)
{
    (void)buffer;
    (void)address;
    (void)count;
    (void)mode;
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *buffer, USHORT address, USHORT count)
{
    (void)buffer;
    (void)address;
    (void)count;
    return MB_ENOREG;
}
