/* Protocol/proto_frame.h - Frame parser and builder API */
#ifndef PROTO_FRAME_H
#define PROTO_FRAME_H

#include <stdint.h>
#include "proto_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ───────────────────────────────────────────────────────── */
#define PROTO_PAYLOAD_MAX   128U

/* Fixed overhead: start(2)+devid(2)+type(1)+cmd(2)+plen(1)+ver(1)+crc(2)+end(2) = 13 bytes
   Each binary byte → 2 ASCII hex chars */
#define PROTO_FRAME_OVERHEAD_BIN  13U
#define PROTO_ASCII_MIN_LEN       (PROTO_FRAME_OVERHEAD_BIN * 2U)  /* 26 chars */
#define PROTO_ASCII_MAX_LEN       ((PROTO_FRAME_OVERHEAD_BIN + PROTO_PAYLOAD_MAX) * 2U + 1U)

/* proto_parse() return codes */
#define PROTO_OK              0
#define PROTO_ERR_TOO_SHORT  -1
#define PROTO_ERR_START_FLAG -2
#define PROTO_ERR_END_FLAG   -3
#define PROTO_ERR_CRC        -4
#define PROTO_ERR_LEN_FIELD  -5
#define PROTO_ERR_BUF_SMALL  -6
#define PROTO_ERR_ODD_LEN    -7

/* ── Frame struct ────────────────────────────────────────────────────── */
typedef struct {
    uint16_t device_id;
    uint8_t  frame_type;
    uint16_t cmd_word;
    uint8_t  proto_ver;
    uint8_t  payload_len;
    uint8_t  payload[PROTO_PAYLOAD_MAX];
} ProtoFrame;

/* ── API ─────────────────────────────────────────────────────────────── */

/*
 * proto_parse - decode ASCII hex string from UART into ProtoFrame
 * @ascii     : null-terminated or length-bounded ASCII hex string
 * @ascii_len : number of ASCII characters to parse (excluding null)
 * @out       : output frame (caller-allocated)
 * Returns: PROTO_OK on success, negative PROTO_ERR_* on failure
 */
int proto_parse(const char *ascii, uint16_t ascii_len, ProtoFrame *out);

/*
 * proto_build - encode ProtoFrame into ASCII hex string for UART transmit
 * @frame    : frame to encode
 * @buf      : output buffer (caller-allocated)
 * @buf_size : output buffer capacity (bytes, must be >= PROTO_ASCII_MAX_LEN)
 * Returns: ASCII string length written (not counting null), 0 on error
 */
uint16_t proto_build(const ProtoFrame *frame, char *buf, uint16_t buf_size);

/*
 * proto_build_ok - build a standard OK ACK frame (payload = {0xFF})
 * Used for commands that require only "OK" acknowledgement.
 */
uint16_t proto_build_ok(uint16_t device_id, uint16_t cmd_word,
                        char *buf, uint16_t buf_size);

/*
 * proto_build_err - build an error frame (frame_type=0xFF)
 * @orig_cmd : original command word being responded to
 */
uint16_t proto_build_err(uint16_t device_id, uint16_t orig_cmd,
                         char *buf, uint16_t buf_size);

/*
 * proto_try_get_device_id - 轻量级解出帧头中的 device_id（K-01 用）
 *   仅校验起始标志 + 解出 device_id，不校验 CRC / 长度 / 结束标志。
 *   用于 CRC 错误或长度不匹配场景，判定是否应回错误帧。
 * @ascii     : ASCII hex 字符串
 * @ascii_len : 长度（至少 8 字符 = start(2B) + devid(2B)）
 * @out_id    : 输出 device_id
 * Returns: 0 成功，-1 起始标志错或长度不足或十六进制非法
 */
int proto_try_get_device_id(const char *ascii, uint16_t ascii_len, uint16_t *out_id);

/*
 * proto_build_ack - build ACK frame with arbitrary payload
 */
uint16_t proto_build_ack(uint16_t device_id, uint16_t cmd_word,
                         const uint8_t *payload, uint8_t payload_len,
                         char *buf, uint16_t buf_size);

/*
 * proto_build_hb - build device heartbeat / power-on notify frame
 *                  (frame_type=0x05, cmd=0x8888, no payload)
 */
uint16_t proto_build_hb(uint16_t device_id, char *buf, uint16_t buf_size);

/*
 * proto_pack_float_be - encode IEEE 754 single-precision into 4 big-endian bytes
 * @value : the float to encode
 * @out   : output[0..3], MSB-first (e.g. 21.59f → 41 AC B8 52)
 */
void proto_pack_float_be(float value, uint8_t *out);

/*
 * proto_unpack_float_be - decode 4 big-endian bytes into IEEE 754 single-precision
 * @in : input[0..3], MSB-first
 * Returns: decoded float
 */
float proto_unpack_float_be(const uint8_t *in);

/*
 * proto_build_auto_report - 组装定时自动上报帧（命令字 0x0302，payload 12B）
 * @device_id : 设备 ID
 * @ts        : UTC 秒级时间戳
 * @ch0       : CH0 浮点（已乘变比）
 * @ch1       : CH1 浮点（已乘变比）
 * @buf       : 输出缓冲
 * @buf_size  : 缓冲容量
 * Returns: ASCII 字符长度，0 表示失败
 */
uint16_t proto_build_auto_report(uint16_t device_id,
                                 uint32_t ts, float ch0, float ch1,
                                 char *buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* PROTO_FRAME_H */
