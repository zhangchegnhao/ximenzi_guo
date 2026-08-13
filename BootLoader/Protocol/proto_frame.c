/* Protocol/proto_frame.c - Frame parser and builder
 * Frames are transmitted as ASCII hex strings per confine.md §2.
 * Example: 0xA5B6 on wire = ASCII chars 'A','5','B','6'.
 * CRC-16-Modbus is computed over binary data (start flag → content inclusive).
 */
#include "proto_frame.h"
#include "proto_crc.h"
#include <stddef.h>

/* ── Fixed frame markers ─────────────────────────────────────────────── */
#define FRAME_START  0xA5B6U
#define FRAME_END    0xB6A5U

/* ── Internal helpers ────────────────────────────────────────────────── */

/* Convert a single hex ASCII char to its 4-bit nibble value.
   Returns 0xFF if the character is not a valid hex digit. */
static uint8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFFU;
}

/* Convert two consecutive ASCII hex chars at src[0..1] to one byte.
   Returns 0 on success, -1 if either char is invalid. */
static int ascii2byte(const char *src, uint8_t *out)
{
    uint8_t hi = hex_nibble(src[0]);
    uint8_t lo = hex_nibble(src[1]);
    if (hi == 0xFFU || lo == 0xFFU) return -1;
    *out = (uint8_t)((hi << 4) | lo);
    return 0;
}

/* Write one byte as two uppercase ASCII hex chars into dst[0..1]. */
static void byte2ascii(uint8_t b, char *dst)
{
    static const char HEX[] = "0123456789ABCDEF";
    dst[0] = HEX[(b >> 4) & 0x0FU];
    dst[1] = HEX[b & 0x0FU];
}

/* ── Lightweight device-id extractor (K-01) ──────────────────────────── */

int proto_try_get_device_id(const char *ascii, uint16_t ascii_len, uint16_t *out_id)
{
    if (ascii == NULL || out_id == NULL) return -1;
    /* 至少需要 start(4 chars) + devid(4 chars) = 8 ASCII chars */
    if (ascii_len < 8U) return -1;

    uint8_t b[4];
    for (uint8_t i = 0U; i < 4U; i++) {
        if (ascii2byte(&ascii[i * 2U], &b[i]) != 0) return -1;
    }
    uint16_t start = ((uint16_t)b[0] << 8) | b[1];
    if (start != FRAME_START) return -1;

    *out_id = ((uint16_t)b[2] << 8) | b[3];
    return 0;
}

/* ── Parse ───────────────────────────────────────────────────────────── */

int proto_parse(const char *ascii, uint16_t ascii_len, ProtoFrame *out)
{
    if (ascii == NULL || out == NULL)
        return PROTO_ERR_TOO_SHORT;

    /* Minimum: 13 binary bytes = 26 ASCII chars */
    if (ascii_len < PROTO_ASCII_MIN_LEN)
        return PROTO_ERR_TOO_SHORT;

    /* Length must be even (each byte = 2 hex chars) */
    if (ascii_len & 1U)
        return PROTO_ERR_ODD_LEN;

    /* Decode entire ASCII string into a temporary binary buffer.
       Max binary size = 13 + PROTO_PAYLOAD_MAX = 141 bytes */
    uint8_t bin[PROTO_FRAME_OVERHEAD_BIN + PROTO_PAYLOAD_MAX];
    uint16_t bin_len = ascii_len / 2U;

    if (bin_len > sizeof(bin))
        return PROTO_ERR_BUF_SMALL;

    for (uint16_t i = 0; i < bin_len; i++) {
        if (ascii2byte(&ascii[i * 2U], &bin[i]) != 0)
            return PROTO_ERR_TOO_SHORT; /* invalid hex char treated as truncated */
    }

    /* ── Validate start flag (bytes 0-1) ── */
    uint16_t start = ((uint16_t)bin[0] << 8) | bin[1];
    if (start != FRAME_START)
        return PROTO_ERR_START_FLAG;

    /* ── Validate end flag (last 2 bytes) ── */
    uint16_t end = ((uint16_t)bin[bin_len - 2U] << 8) | bin[bin_len - 1U];
    if (end != FRAME_END)
        return PROTO_ERR_END_FLAG;

    /* ── Extract fixed header fields ──
       Byte offsets in binary frame:
         0-1  : start flag
         2-3  : device ID
         4    : frame type
         5-6  : command word
         7    : payload length
         8    : protocol version
         9..  : payload
         last4: CRC(2) + end flag(2)                                */
    uint8_t payload_len = bin[7];

    /* Sanity: total binary should be 13 + payload_len */
    if (bin_len != (uint16_t)(PROTO_FRAME_OVERHEAD_BIN + payload_len))
        return PROTO_ERR_LEN_FIELD;

    if (payload_len > PROTO_PAYLOAD_MAX)
        return PROTO_ERR_BUF_SMALL;

    /* ── Validate CRC ── (covers bytes 0 .. 9+payload_len-1) */
    uint16_t crc_range_len = (uint16_t)(PROTO_FRAME_OVERHEAD_BIN - 4U + payload_len);
    /* overhead 13 - crc(2) - end(2) = 9 base + payload_len */
    uint16_t crc_calc = proto_crc16(bin, crc_range_len);
    uint16_t crc_recv = ((uint16_t)bin[crc_range_len] << 8) | bin[crc_range_len + 1U];

    if (crc_calc != crc_recv)
        return PROTO_ERR_CRC;

    /* ── Populate output struct ── */
    out->device_id   = ((uint16_t)bin[2] << 8) | bin[3];
    out->frame_type  = bin[4];
    out->cmd_word    = ((uint16_t)bin[5] << 8) | bin[6];
    out->payload_len = payload_len;
    out->proto_ver   = bin[8];

    for (uint8_t i = 0; i < payload_len; i++)
        out->payload[i] = bin[9U + i];

    return PROTO_OK;
}

/* ── Build ───────────────────────────────────────────────────────────── */

uint16_t proto_build(const ProtoFrame *frame, char *buf, uint16_t buf_size)
{
    if (frame == NULL || buf == NULL)
        return 0U;

    if (frame->payload_len > PROTO_PAYLOAD_MAX)
        return 0U;

    uint16_t bin_len = PROTO_FRAME_OVERHEAD_BIN + frame->payload_len;
    uint16_t ascii_need = bin_len * 2U + 1U; /* +1 for null terminator */

    if (buf_size < ascii_need)
        return 0U;

    /* Build binary frame into stack buffer */
    uint8_t bin[PROTO_FRAME_OVERHEAD_BIN + PROTO_PAYLOAD_MAX];

    /* Start flag */
    bin[0] = (uint8_t)(FRAME_START >> 8);
    bin[1] = (uint8_t)(FRAME_START & 0xFFU);
    /* Device ID */
    bin[2] = (uint8_t)(frame->device_id >> 8);
    bin[3] = (uint8_t)(frame->device_id & 0xFFU);
    /* Frame type */
    bin[4] = frame->frame_type;
    /* Command word */
    bin[5] = (uint8_t)(frame->cmd_word >> 8);
    bin[6] = (uint8_t)(frame->cmd_word & 0xFFU);
    /* Payload length */
    bin[7] = frame->payload_len;
    /* Protocol version */
    bin[8] = frame->proto_ver ? frame->proto_ver : PROTO_VERSION;
    /* Payload */
    for (uint8_t i = 0; i < frame->payload_len; i++)
        bin[9U + i] = frame->payload[i];

    /* CRC covers bytes 0 .. 9+payload_len-1 */
    uint16_t crc_range = (uint16_t)(9U + frame->payload_len);
    uint16_t crc = proto_crc16(bin, crc_range);

    uint16_t pos = crc_range;
    bin[pos++] = (uint8_t)(crc >> 8);
    bin[pos++] = (uint8_t)(crc & 0xFFU);
    /* End flag */
    bin[pos++] = (uint8_t)(FRAME_END >> 8);
    bin[pos++] = (uint8_t)(FRAME_END & 0xFFU);

    /* Encode binary → ASCII hex */
    for (uint16_t i = 0; i < bin_len; i++)
        byte2ascii(bin[i], &buf[i * 2U]);

    buf[bin_len * 2U] = '\0';
    return (uint16_t)(bin_len * 2U);
}

/* ── Convenience builders ────────────────────────────────────────────── */

uint16_t proto_build_ack(uint16_t device_id, uint16_t cmd_word,
                         const uint8_t *payload, uint8_t payload_len,
                         char *buf, uint16_t buf_size)
{
    ProtoFrame f;
    f.device_id   = device_id;
    f.frame_type  = PROTO_TYPE_ACK;
    f.cmd_word    = cmd_word;
    f.proto_ver   = PROTO_VERSION;
    f.payload_len = payload_len;
    for (uint8_t i = 0; i < payload_len; i++)
        f.payload[i] = payload[i];
    return proto_build(&f, buf, buf_size);
}

uint16_t proto_build_ok(uint16_t device_id, uint16_t cmd_word,
                        char *buf, uint16_t buf_size)
{
    uint8_t ok = PROTO_ACK_OK;
    return proto_build_ack(device_id, cmd_word, &ok, 1U, buf, buf_size);
}

uint16_t proto_build_err(uint16_t device_id, uint16_t orig_cmd,
                         char *buf, uint16_t buf_size)
{
    ProtoFrame f;
    f.device_id   = device_id;
    f.frame_type  = PROTO_TYPE_ERR;
    f.cmd_word    = orig_cmd;
    f.proto_ver   = PROTO_VERSION;
    f.payload_len = 0U;
    return proto_build(&f, buf, buf_size);
}

uint16_t proto_build_hb(uint16_t device_id, char *buf, uint16_t buf_size)
{
    ProtoFrame f;
    f.device_id   = device_id;
    f.frame_type  = PROTO_TYPE_HB;
    f.cmd_word    = CMD_HB_DEVICE;
    f.proto_ver   = PROTO_VERSION;
    f.payload_len = 0U;
    return proto_build(&f, buf, buf_size);
}

void proto_pack_float_be(float value, uint8_t *out)
{
    /* IEEE 754 单精度 4 字节，Cortex-M4 为小端，按字节倒序得大端输出 */
    union { float f; uint8_t b[4]; } u;
    u.f = value;
    out[0] = u.b[3];
    out[1] = u.b[2];
    out[2] = u.b[1];
    out[3] = u.b[0];
}

float proto_unpack_float_be(const uint8_t *in)
{
    union { float f; uint8_t b[4]; } u;
    u.b[0] = in[3];
    u.b[1] = in[2];
    u.b[2] = in[1];
    u.b[3] = in[0];
    return u.f;
}

uint16_t proto_build_auto_report(uint16_t device_id,
                                 uint32_t ts, float ch0, float ch1,
                                 char *buf, uint16_t buf_size)
{
    uint8_t payload[12];
    /* UTC 秒时间戳 4 字节大端 */
    payload[0] = (uint8_t)((ts >> 24) & 0xFFU);
    payload[1] = (uint8_t)((ts >> 16) & 0xFFU);
    payload[2] = (uint8_t)((ts >> 8)  & 0xFFU);
    payload[3] = (uint8_t)( ts        & 0xFFU);
    /* CH0 / CH1 IEEE 754 大端 */
    proto_pack_float_be(ch0, &payload[4]);
    proto_pack_float_be(ch1, &payload[8]);
    return proto_build_ack(device_id, CMD_AUTO_REPORT_ON,
                           payload, 12U, buf, buf_size);
}
