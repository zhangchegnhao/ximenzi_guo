#include "bl_crc32.h"

/* Reflected CRC32 algorithm used by APP OTA metadata and BootLoader checks. */
uint32_t bl_crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc;
    uint32_t i;
    uint32_t j;

    crc = 0xFFFFFFFFUL;
    for(i = 0UL; i < len; i++) {
        crc ^= data[i];
        for(j = 0UL; j < 8UL; j++) {
            if((crc & 1UL) != 0UL) {
                crc = (crc >> 1UL) ^ 0xEDB88320UL;
            } else {
                crc >>= 1UL;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}
