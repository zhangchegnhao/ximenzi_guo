#ifndef PROTOCOL_BL_CRC32_H
#define PROTOCOL_BL_CRC32_H

#include <stdint.h>

uint32_t bl_crc32_calc(const uint8_t *data, uint32_t len);

#endif /* PROTOCOL_BL_CRC32_H */
