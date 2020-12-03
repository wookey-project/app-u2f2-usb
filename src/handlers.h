#ifndef HANDLERS_H_
#define HANDLERS_H_

#include "libc/types.h"

#define MAGIC_WINK_REQ          0x42420000UL

#define MAGIC_APDU_CMD_INIT     0xa5a50001UL /* ask for initiate APDU Cmd */
#define MAGIC_APDU_CMD_META     0xa5a50002UL /* send apdu cmd metadata */
#define MAGIC_APDU_CMD_MSG_LEN  0xa5a50003UL /* send apdu cmd buffer len (in bytes) */
#define MAGIC_APDU_CMD_MSG      0xa5a50004UL /* send apdu cmd buffer (len / 64 messages number + residual) */

#define MAGIC_APDU_RESP_INIT    0x5a5a0001UL /* ask for initiate APDU response */
#define MAGIC_APDU_RESP_MSG_LEN 0x5a5a0002UL /* send apdu response buffer len (in bytes) */
#define MAGIC_APDU_RESP_MSG     0x5a5a0003UL /* send apdu response buffer (len / 64 messages number + residual) */

#define MAGIC_CMD_RETURN        0xdeadbeefUL /* remote command return value */

#define MAGIC_ACKNOWLEDGE       0xeba42148UL /* acknowledge a command */

/*
 * Local handlers to FIDO events
 */

mbed_error_t handle_wink(uint16_t timeout_ms);

mbed_error_t u2fapdu_handle_cmd(uint32_t metadata, uint8_t *buf, uint16_t buf_len, uint8_t *resp, uint16_t *resp_len);

#endif/*HANDLERS_H_*/
