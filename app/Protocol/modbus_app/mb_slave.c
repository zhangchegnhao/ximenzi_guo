#include "mb_slave.h"
#include "mb_registers.h"

#define MB_SLAVE_ADDRESS  1U
#define MB_SLAVE_BAUDRATE 115200UL

eMBErrorCode mb_slave_init(void)
{
    eMBErrorCode status;

    mb_registers_init();
    status = eMBInit(MB_RTU, MB_SLAVE_ADDRESS, 0U,
                     MB_SLAVE_BAUDRATE, MB_PAR_NONE);
    if (status == MB_ENOERR) {
        status = eMBEnable();
    }
    return status;
}

void mb_slave_poll(void)
{
    (void)eMBPoll();
}
