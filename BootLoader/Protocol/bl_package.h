#ifndef PROTOCOL_BL_PACKAGE_H
#define PROTOCOL_BL_PACKAGE_H

#include <stdbool.h>
#include <stdint.h>

/* Upgrade package format: 4-byte magic followed by the raw application image. */
#define BL_FW_PACKAGE_MAGIC          0x5AA5C33CUL
#define BL_FW_PACKAGE_HEADER_SIZE    4UL
#define BL_FW_PACKAGE_MAGIC_BYTE0    0x5AU
#define BL_FW_PACKAGE_MAGIC_BYTE1    0xA5U
#define BL_FW_PACKAGE_MAGIC_BYTE2    0xC3U
#define BL_FW_PACKAGE_MAGIC_BYTE3    0x3CU

static inline uint32_t bl_package_firmware_addr(uint32_t package_addr)
{
    return package_addr + BL_FW_PACKAGE_HEADER_SIZE;
}

static inline uint32_t bl_package_firmware_size(uint32_t package_size)
{
    return package_size - BL_FW_PACKAGE_HEADER_SIZE;
}

static inline bool bl_package_magic_match(uint32_t package_addr)
{
    const volatile uint8_t *magic = (const volatile uint8_t *)package_addr;

    return (magic[0] == BL_FW_PACKAGE_MAGIC_BYTE0) &&
           (magic[1] == BL_FW_PACKAGE_MAGIC_BYTE1) &&
           (magic[2] == BL_FW_PACKAGE_MAGIC_BYTE2) &&
           (magic[3] == BL_FW_PACKAGE_MAGIC_BYTE3);
}

#endif /* PROTOCOL_BL_PACKAGE_H */
