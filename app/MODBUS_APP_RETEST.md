# USART1 Modbus APP Retest

The main APP currently defaults to `APP_COMM_MODE_MODBUS_RTU`. USART1 and the
RS485 transceiver are owned by FreeModbus in this build.

## Firmware

- Keil project: `MDK/Project.uvprojx`
- HEX: `MDK/output_modbus/Project.hex`
- APP load address: `0x08011000`

This is an APP1 image. Keep the existing BootLoader and parameter page when
downloading it. Do not treat this HEX as the standalone baseline image that
starts at `0x08000000`.

## Modbus Poll

- Mode: Modbus RTU
- Slave address: `1`
- Serial: `115200-8-N-1`
- Function: `0x04` Read Input Registers
- Start address: `0`
- Quantity: `6`
- Initial scan interval: `500 ms`

Expected values:

```text
0x1234 0x5678 0x0001 0x0002 0x0003 0x0004
```

Expected request and response:

```text
Request:  01 04 00 00 00 06 70 08
Response: 01 04 0C 12 34 56 78 00 01 00 02 00 03 00 04 3E 20
```

## Acceptance

1. Read all six registers successfully.
2. Verify address `5`, quantity `2` returns exception `02`.
3. Verify slave address `2` receives no response.
4. Run 1000 reads at a `100 ms` interval without CRC errors or timeouts.
5. Confirm sampling, OLED, RTC, and storage tasks continue running.

To restore the private ASCII protocol, change `APP_COMM_MODE` in
`common/app_comm_config.h` to `APP_COMM_MODE_PRIVATE_ASCII`, rebuild, and
download the resulting APP1 image.
