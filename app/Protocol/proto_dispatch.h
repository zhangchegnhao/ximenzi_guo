/* Protocol/proto_dispatch.h - Command dispatcher */
#ifndef PROTO_DISPATCH_H
#define PROTO_DISPATCH_H

#include <stdint.h>
#include "proto_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * proto_dispatch - parse an incoming ASCII frame, dispatch to the appropriate
 *                  handler, and write the ASCII response into resp_buf.
 * @ascii    : raw ASCII hex string received from UART (not null-terminated required)
 * @len      : number of ASCII characters received
 * @resp_buf : caller-provided output buffer for the response ASCII frame
 * @buf_size : size of resp_buf
 * Returns: length of ASCII response written (0 = no response / parse error)
 */
uint16_t proto_dispatch(const char *ascii, uint16_t len,
                        char *resp_buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* PROTO_DISPATCH_H */
