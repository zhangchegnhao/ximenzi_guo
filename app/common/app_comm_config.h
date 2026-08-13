#ifndef APP_COMM_CONFIG_H
#define APP_COMM_CONFIG_H

#define APP_COMM_MODE_PRIVATE_ASCII  1
#define APP_COMM_MODE_MODBUS_RTU     2

/* USART1/RS485 has one owner per firmware build. */
#ifndef APP_COMM_MODE
#define APP_COMM_MODE APP_COMM_MODE_MODBUS_RTU
#endif

#if (APP_COMM_MODE != APP_COMM_MODE_PRIVATE_ASCII) && \
    (APP_COMM_MODE != APP_COMM_MODE_MODBUS_RTU)
#error "Invalid APP_COMM_MODE"
#endif

#endif /* APP_COMM_CONFIG_H */
