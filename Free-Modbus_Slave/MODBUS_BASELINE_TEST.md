# FreeModbus RTU Baseline Test

This project is intentionally limited to one read-only Modbus operation so the
GD32F470VET6, USART1, RS485 direction control, and TIMER6 port can be verified
before integrating FreeModbus into the main application.

## Configuration

- Mode: Modbus RTU
- Slave address: `1`
- Serial: `115200-8-N-1`
- Supported function: `0x04` Read Input Registers
- Register range: protocol addresses `0` through `5`

## Register Values

| Protocol address | Value |
|---:|---:|
| 0 | `0x1234` |
| 1 | `0x5678` |
| 2 | `0x0001` |
| 3 | `0x0002` |
| 4 | `0x0003` |
| 5 | `0x0004` |

Some Modbus master tools display protocol address 0 as register 30001. Disable
one-based addressing in the tool or account for that display offset.

## Expected Frames

Read all six registers:

```text
Request:  01 04 00 00 00 06 70 08
Response: 01 04 0C 12 34 56 78 00 01 00 02 00 03 00 04 3E 20
```

An out-of-range read must return exception 02. For example, a request that is
otherwise valid but exceeds the six-register range produces:

```text
Exception response: 01 84 02 C2 C1
```

## Hardware Check

1. Build and download `project/Objects/CIMC_GD32_Template.hex`.
2. Connect the board USART1 RS485 A/B lines to a USB-RS485 adapter.
3. Configure the master for slave 1 and `115200-8-N-1`.
4. Read six input registers starting at protocol address 0.
5. Repeat at least 1000 times and verify no CRC errors or timeouts.
6. Read a range beyond address 5 and verify Modbus exception 02.
7. Query another slave address and verify that the board remains silent.

This baseline is aligned with the current main-application BSP: USART1 uses
PA2/PA3 and the RS485 direction pin is PA1.
