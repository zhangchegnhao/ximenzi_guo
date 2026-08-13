/* Protocol/proto_dispatch.c
 * Parses incoming ASCII frames, routes each command to the Function layer,
 * and builds the ASCII response. No business logic lives here.
 */
#include "proto_dispatch.h"
#include "proto_frame.h"
#include "proto_cmd.h"
#include "sys_device.h"
#include "sys_time.h"
#include "sys_sample.h"
#include "sys_pt100.h"
#include "sys_param.h"
#include "sys_dac.h"
#include "sys_report.h"
#include "sys_alarm.h"
#include "sys_sleep.h"
#include "sys_baudrate.h"
#include "sys_ota.h"
#include "ota_uart.h"
#include "usart_app.h"               /* rs_usart_send for plain-ASCII responses */
#include "mcu_cimc_gd32f470vet6.h"  /* for get_system_ms */

uint16_t proto_dispatch(const char *ascii, uint16_t len,
                        char *resp_buf, uint16_t buf_size)
{
    ProtoFrame rx;
    uint16_t my_id = sys_device_get_id();
    int parse_rc = proto_parse(ascii, len, &rx);

    /* ── K-01: 解析失败时的错误应答处理 ──
       仅 CRC 错误和长度字段不匹配两种情况，且能解出 device_id 并匹配本机/广播，
       才回错误帧；其余（起止标志错、太短、奇长、非法 hex）静默丢弃。
       自动上报期间静默，避免干扰上报流。*/
    if (parse_rc != PROTO_OK) {
        if (sys_report_is_enabled()) return 0U;
        if (parse_rc == PROTO_ERR_CRC || parse_rc == PROTO_ERR_LEN_FIELD) {
            uint16_t dev_id;
            if (proto_try_get_device_id(ascii, len, &dev_id) == 0
                && (dev_id == PROTO_DEV_BROADCAST || dev_id == my_id)) {
                return proto_build_err(my_id, CMD_ERR_WORD,
                                       resp_buf, buf_size);
            }
        }
        return 0U;
    }

    /* Ignore frames not addressed to this device or broadcast */
    if (rx.device_id != PROTO_DEV_BROADCAST && rx.device_id != my_id)
        return 0U;

    /* H-02: 自动上报启用期间，只接受 0x0303（停止），其余命令一律忽略 */
    if (sys_report_is_enabled()
        && !(rx.frame_type == PROTO_TYPE_CMD
             && rx.cmd_word == CMD_AUTO_REPORT_OFF)) {
        return 0U;
    }

    /* ── K-01: 未知帧类型 → 错误帧 + 日志（仅 CMD/HB 为合法入向帧类型） ── */
    if (rx.frame_type != PROTO_TYPE_CMD && rx.frame_type != PROTO_TYPE_HB) {
        my_printf(DEBUG_USART,
                  "[K-01] unknown frame type 0x%X devid 0x%X cmd 0x%X\r\n",
                  rx.frame_type, rx.device_id, rx.cmd_word);
        return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
    }

    /* ── A-01/A-03: Broadcast scan → reply with device heartbeat frame ─ */
    if (rx.frame_type == PROTO_TYPE_HB && rx.cmd_word == CMD_HB_PC_SCAN) {
        return proto_build_hb(my_id, resp_buf, buf_size);
    }

    /* ── A-02: Device reboot ─────────────────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_REBOOT) {
        uint16_t len_out = proto_build_ok(my_id, CMD_REBOOT, resp_buf, buf_size);
        sys_device_request_reboot();
        return len_out;
    }

    /* ── N-01: Enter BootLoader for OTA upgrade request ────────────────
       无 payload；先回 OK，uart_task 发完后软复位，复位后由 BootLoader
       停在串口升级包接收流程。 */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_OTA_REQUEST) {
        if (rx.payload_len != 0U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        uint16_t len_out = proto_build_ok(my_id, CMD_OTA_REQUEST,
                                          resp_buf, buf_size);
        /* 跨复位标志：让 BL 启动时知道这是 APP 主动触发的升级请求，
           走 10s 倒计时 + 等 0x0502 路径；Flash 擦写延后到 OK 发出后。 */
        ota_request_fast_upgrade_mark();
        sys_device_request_reboot();
        return len_out;
    }

    /* ── N-02: OTA 准备传输（错误固件验证） ─────────────────────────────
       无 payload；不立即应答。进入 sys_ota 验证态后：
         - RS485 后续字节由 ota_uart_process_frame 守卫转交 sys_ota_feed
         - 仅缓存前 4B 用于魔术字（5A A5 C3 3C）校验
         - 100ms 空闲超时视为"接收完成"，由 uart_task 取出应答发出
         - 魔术字匹配 → OK 帧（payload=FF）；否则 → FF + 0x0502 错误帧
       同时 reset 现有 ota_uart 流式接收，避免之前残留字节误触发安装流程。 */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_OTA_PREPARE) {
        if (rx.payload_len != 0U) {
            return proto_build_err(my_id, CMD_OTA_PREPARE, resp_buf, buf_size);
        }
        ota_uart_reset_state();
        sys_ota_request_verify();
        return 0U;  /* 不立即应答 */
    }

    /* ── B-01: Query firmware version ────────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_QUERY_FW_VER) {
        uint8_t ver[SYS_DEVICE_FW_VER_LEN];
        sys_device_get_fw_version(ver);
        return proto_build_ack(my_id, CMD_QUERY_FW_VER,
                               ver, SYS_DEVICE_FW_VER_LEN,
                               resp_buf, buf_size);
    }

    /* ── B-02: Query device time (UTC seconds, 4 bytes big-endian) ───── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_GET_TIME) {
        uint32_t ts = sys_time_get_utc_seconds();
        uint8_t payload[4];
        payload[0] = (uint8_t)((ts >> 24) & 0xFFU);
        payload[1] = (uint8_t)((ts >> 16) & 0xFFU);
        payload[2] = (uint8_t)((ts >> 8)  & 0xFFU);
        payload[3] = (uint8_t)(ts & 0xFFU);
        return proto_build_ack(my_id, CMD_GET_TIME,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── C-01: Set device time (4-byte UTC seconds big-endian → OK) ──── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_TIME) {
        if (rx.payload_len != 4U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        uint32_t ts = ((uint32_t)rx.payload[0] << 24)
                    | ((uint32_t)rx.payload[1] << 16)
                    | ((uint32_t)rx.payload[2] << 8)
                    |  (uint32_t)rx.payload[3];
        if (sys_time_set_utc_seconds(ts) != 0) {
            return proto_build_err(my_id, CMD_SET_TIME, resp_buf, buf_size);
        }
        return proto_build_ok(my_id, CMD_SET_TIME, resp_buf, buf_size);
    }

    /* ── B-03: Query baudrate (1-byte mapping code) ──────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_GET_BAUDRATE) {
        uint8_t code = sys_device_get_baudrate_code();
        return proto_build_ack(my_id, CMD_GET_BAUDRATE,
                               &code, 1U, resp_buf, buf_size);
    }

    /* ── B-04: Query CH0 (potentiometer) data, IEEE 754 BE float ─────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_CH0) {
        uint8_t payload[4];
        proto_pack_float_be(sys_sample_get_ch0(), payload);
        return proto_build_ack(my_id, CMD_READ_CH0,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── B-05: Query CH1 (DAC readback) data, IEEE 754 BE float ──────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_CH1) {
        uint8_t payload[4];
        proto_pack_float_be(sys_sample_get_ch1(), payload);
        return proto_build_ack(my_id, CMD_READ_CH1,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── CH2 (PT100): Query temperature in °C, IEEE 754 BE float ─────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_CH2_PT100) {
        uint8_t payload[4];
        proto_pack_float_be(sys_pt100_get_temp(), payload);
        return proto_build_ack(my_id, CMD_READ_CH2_PT100,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── B-06: Query thresholds for CH0+CH1 (8 bytes, 2 BE floats) ───── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_THRESH_ALL) {
        uint8_t payload[8];
        proto_pack_float_be(sys_param_get_ch0_thresh(), &payload[0]);
        proto_pack_float_be(sys_param_get_ch1_thresh(), &payload[4]);
        return proto_build_ack(my_id, CMD_READ_THRESH_ALL,
                               payload, 8U, resp_buf, buf_size);
    }

    /* ── D-01: Set CH0 ratio (4-byte IEEE 754 BE → OK) ───────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_RATIO_CH0) {
        if (rx.payload_len != 4U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        sys_param_set_ch0_ratio(proto_unpack_float_be(rx.payload));
        return proto_build_ok(my_id, CMD_SET_RATIO_CH0, resp_buf, buf_size);
    }

    /* ── D-02: Set CH1 ratio (4-byte IEEE 754 BE → OK) ───────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_RATIO_CH1) {
        if (rx.payload_len != 4U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        sys_param_set_ch1_ratio(proto_unpack_float_be(rx.payload));
        return proto_build_ok(my_id, CMD_SET_RATIO_CH1, resp_buf, buf_size);
    }

    /* ── E-01a: Read CH0 threshold (single-channel) ──────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_THRESH_CH0) {
        uint8_t payload[4];
        proto_pack_float_be(sys_param_get_ch0_thresh(), payload);
        return proto_build_ack(my_id, CMD_READ_THRESH_CH0,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── E-01b: Write CH0 threshold ──────────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_WRITE_THRESH_CH0) {
        if (rx.payload_len != 4U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        sys_param_set_ch0_thresh(proto_unpack_float_be(rx.payload));
        return proto_build_ok(my_id, CMD_WRITE_THRESH_CH0, resp_buf, buf_size);
    }

    /* ── E-02a: Read CH1 threshold (single-channel) ──────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_READ_THRESH_CH1) {
        uint8_t payload[4];
        proto_pack_float_be(sys_param_get_ch1_thresh(), payload);
        return proto_build_ack(my_id, CMD_READ_THRESH_CH1,
                               payload, 4U, resp_buf, buf_size);
    }

    /* ── E-02b: Write CH1 threshold ──────────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_WRITE_THRESH_CH1) {
        if (rx.payload_len != 4U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        sys_param_set_ch1_thresh(proto_unpack_float_be(rx.payload));
        return proto_build_ok(my_id, CMD_WRITE_THRESH_CH1, resp_buf, buf_size);
    }

    /* ── G-01: Set DAC output (2-byte raw 0x0000~0x0FFF → OK) ────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_DAC) {
        if (rx.payload_len != 2U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        uint16_t raw = ((uint16_t)rx.payload[0] << 8) | rx.payload[1];
        sys_dac_set_raw(raw);
        return proto_build_ok(my_id, CMD_SET_DAC, resp_buf, buf_size);
    }

    /* ── H-01a: Set auto-report interval (1B: 01=1s, 02=3s, 03=5s) ───── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_REPORT_INTV) {
        if (rx.payload_len != 1U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        if (sys_report_set_interval_code(rx.payload[0]) != 0) {
            return proto_build_err(my_id, CMD_SET_REPORT_INTV, resp_buf, buf_size);
        }
        return proto_build_ok(my_id, CMD_SET_REPORT_INTV, resp_buf, buf_size);
    }

    /* ── H-01b: Start auto-report (响应即首帧报告，不是 OK) ──────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_AUTO_REPORT_ON) {
        uint32_t now = get_system_ms();
        sys_report_start(now);
        return proto_build_auto_report(my_id,
                                       sys_time_get_utc_seconds(),
                                       sys_sample_get_ch0(),
                                       sys_sample_get_ch1(),
                                       resp_buf, buf_size);
    }

    /* ── H-01c: Stop auto-report ─────────────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_AUTO_REPORT_OFF) {
        sys_report_stop();
        return proto_build_ok(my_id, CMD_AUTO_REPORT_OFF, resp_buf, buf_size);
    }

    /* ── J-01: Enter deep sleep (10s RTC alarm wakeup) ───────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SLEEP) {
        uint16_t len_out = proto_build_ok(my_id, CMD_SLEEP, resp_buf, buf_size);
        sys_sleep_request(10U);   /* 应答发完后由 uart_task 触发睡眠 */
        return len_out;
    }

    /* ── I-01: Set alarm reporting mode (1B: 01=active, 02=passive) ──── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_ALARM_REPORT_EN) {
        if (rx.payload_len != 1U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        if (sys_alarm_set_mode(rx.payload[0]) != 0) {
            return proto_build_err(my_id, CMD_ALARM_REPORT_EN, resp_buf, buf_size);
        }
        return proto_build_ok(my_id, CMD_ALARM_REPORT_EN, resp_buf, buf_size);
    }

    /* ── I-04a: Query alarm records (最多 10 条，按时间倒序) ─────────────
       应答为纯 ASCII 字符串（非帧封装），逐行直接发送；无记录则回 "empty\n"。 */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_ALARM_QUERY) {
        uint16_t total = sys_alarm_count();
        if (total == 0U) {
            rs_usart_send("empty\n", 6U);
        } else {
            uint16_t n = (total < 10U) ? total : 10U;
            char line[80];
            for (uint16_t i = 0U; i < n; i++) {
                SysAlarmRecord rec;
                /* 倒序：最新一条 idx=total-1，倒数第二 idx=total-2 ... */
                if (sys_alarm_get_record((uint16_t)(total - 1U - i), &rec) == 0) {
                    uint16_t len = sys_alarm_format_record(&rec, line, sizeof(line));
                    if (len > 0U) rs_usart_send(line, len);
                }
            }
        }
        return 0U;  /* dispatch 已自行发送，告诉上层不要再发 */
    }

    /* ── I-04b: Clear all alarm records ──────────────────────────────── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_ALARM_CLEAR) {
        sys_alarm_clear_all();
        return proto_build_ok(my_id, CMD_ALARM_CLEAR, resp_buf, buf_size);
    }

    /* ── M-01: Set baudrate (1B code: 11=4800/12=9600/13=19200/14=115200)
       先回 OK，再持久化 + 触发延迟重启；重启后 main 用持久化码恢复 USART。 */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_BAUDRATE) {
        if (rx.payload_len != 1U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        /* 仅做合法性校验，不立即应用（先发完 OK 再重启） */
        if (sys_baudrate_code_to_value(rx.payload[0]) == 0U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        sys_device_set_baudrate_code(rx.payload[0]); /* 自动持久化 */
        uint16_t len_out = proto_build_ok(my_id, CMD_SET_BAUDRATE,
                                          resp_buf, buf_size);
        sys_device_request_reboot();                 /* uart_task 发完 OK 后执行 */
        return len_out;
    }

    /* ── L-02: Query device ID (应答 2B 当前 device_id，下发常用广播 FFFF) ── */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_GET_DEV_ID) {
        uint8_t payload[2];
        payload[0] = (uint8_t)((my_id >> 8) & 0xFFU);
        payload[1] = (uint8_t)(my_id & 0xFFU);
        return proto_build_ack(my_id, CMD_GET_DEV_ID,
                               payload, 2U, resp_buf, buf_size);
    }

    /* ── L-01: Set device ID (2B big-endian, valid range 0x0001~0xFFFE) ──
       - 长度错 → K-02 错误帧
       - ID 值非法（0x0000 / 0xFFFF）→ 静默丢弃，不应答
       - 合法 → 调 setter（自动持久化），应答 OK，**device_id 字段用新 ID** */
    if (rx.frame_type == PROTO_TYPE_CMD && rx.cmd_word == CMD_SET_DEV_ID) {
        if (rx.payload_len != 2U) {
            return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
        }
        uint16_t new_id = ((uint16_t)rx.payload[0] << 8) | rx.payload[1];
        if (new_id == 0x0000U || new_id == PROTO_DEV_BROADCAST) {
            return 0U;  /* ID 错误 → 不应答 */
        }
        sys_device_set_id(new_id);
        return proto_build_ok(new_id, CMD_SET_DEV_ID, resp_buf, buf_size);
    }

    /* ── K-03: 走到此处说明帧类型合法但 cmd_word 未实现 → 错误帧 + 日志 ── */
    my_printf(DEBUG_USART,
              "[K-03] illegal cmd 0x%X type 0x%X devid 0x%X\r\n",
              rx.cmd_word, rx.frame_type, rx.device_id);
    return proto_build_err(my_id, CMD_ERR_WORD, resp_buf, buf_size);
}
