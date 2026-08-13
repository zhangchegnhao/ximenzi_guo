/* Protocol/proto_crc.h - CRC-16-Modbus software implementation */
#ifndef PROTO_CRC_H
#define PROTO_CRC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRC-16-Modbus over binary data, init=0xFFFF, poly=0xA001 (reflected 0x8005) */
uint16_t proto_crc16(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* PROTO_CRC_H */
