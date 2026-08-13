/* Protocol/proto_cmd.h - Frame type and command word constants */
#ifndef PROTO_CMD_H
#define PROTO_CMD_H

#include <stdint.h>

/* ── Frame types ──────────────────────────────────────────────────────── */
#define PROTO_TYPE_CMD   0x01U   /* PC → device: command */
#define PROTO_TYPE_ACK   0x02U   /* device → PC: response */
#define PROTO_TYPE_HB    0x05U   /* bidirectional: heartbeat */
#define PROTO_TYPE_ERR   0xFFU   /* device → PC: error / alarm */

/* ACK OK single-byte payload value */
#define PROTO_ACK_OK     0xFFU

/* Broadcast device ID */
#define PROTO_DEV_BROADCAST  0xFFFFU

/* Protocol version */
#define PROTO_VERSION    0x02U

/* ── System management (0x01xx) ───────────────────────────────────────── */
#define CMD_REBOOT           0x0101U  /* device reboot */
#define CMD_FACTORY_RESET    0x0102U  /* factory reset (reserved) */
#define CMD_QUERY_DEV_INFO   0x0103U  /* query device info (reserved) */
#define CMD_QUERY_FW_VER     0x0104U  /* query firmware version */
#define CMD_SET_TIME         0x0105U  /* set device time */
#define CMD_GET_TIME         0x0106U  /* query device time */
#define CMD_SET_DEV_ID       0x01A1U  /* set device ID */
#define CMD_SET_BAUDRATE     0x01A2U  /* set baud rate */
#define CMD_GET_DEV_ID       0x0111U  /* query device ID */
#define CMD_GET_BAUDRATE     0x0112U  /* query baud rate */

/* ── Data class (0x02xx) ─────────────────────────────────────────────── */
#define CMD_READ_CH0         0x0201U  /* query CH0 ADC (potentiometer) */
#define CMD_READ_CH1         0x0202U  /* query CH1 ADC (DAC readback) */
#define CMD_READ_CH2_PT100   0x0221U  /* query external ADC PT100 */
#define CMD_SET_RATIO_CH0    0x0241U  /* set CH0 ratio */
#define CMD_SET_RATIO_CH1    0x0242U  /* set CH1 ratio */
#define CMD_SET_REPORT_INTV  0x0261U  /* set auto-report interval */

/* ── Control class (0x03xx) ──────────────────────────────────────────── */
#define CMD_SET_DAC          0x0301U  /* set DAC output voltage */
#define CMD_AUTO_REPORT_ON   0x0302U  /* start timed auto-report (CH0+CH1) */
#define CMD_AUTO_REPORT_OFF  0x0303U  /* stop timed auto-report */
#define CMD_SLEEP            0x03AAU  /* enter sleep mode */

/* ── Parameter config (0x04xx) ───────────────────────────────────────── */
#define CMD_READ_THRESH_ALL  0x0400U  /* read CH0+CH1 thresholds */
#define CMD_READ_THRESH_CH0  0x0401U  /* read CH0 threshold */
#define CMD_READ_THRESH_CH1  0x0402U  /* read CH1 threshold */
#define CMD_READ_THRESH_CH2  0x0403U  /* read CH2 threshold */
#define CMD_WRITE_THRESH_CH0 0x0411U  /* write CH0 threshold */
#define CMD_WRITE_THRESH_CH1 0x0412U  /* write CH1 threshold */
#define CMD_WRITE_THRESH_CH2 0x0413U  /* write CH2 threshold */

/* ── OTA upgrade (0x05xx) ────────────────────────────────────────────── */
#define CMD_OTA_REQUEST      0x0501U  /* upgrade request */
#define CMD_OTA_PREPARE      0x0502U  /* prepare firmware transfer */
#define CMD_OTA_EXECUTE      0x0503U  /* execute upgrade */

/* ── Alarm & log (0x06xx) ────────────────────────────────────────────── */
#define CMD_ALARM_REPORT_EN  0x0601U  /* enable active alarm report */
#define CMD_ALARM_QUERY      0x0602U  /* query alarm records */
#define CMD_ALARM_CLEAR      0x0603U  /* clear alarms */
#define CMD_LOG_QUERY        0x0604U  /* query op log (reserved) */
#define CMD_LOG_CLEAR        0x0605U  /* clear op log (reserved) */

/* ── Special command words ───────────────────────────────────────────── */
#define CMD_HB_DEVICE        0x8888U  /* device heartbeat / power-on notify */
#define CMD_HB_PC_SCAN       0xFFFFU  /* PC broadcast scan */
#define CMD_ERR_WORD         0xEEEEU  /* generic error response */

#endif /* PROTO_CMD_H */
