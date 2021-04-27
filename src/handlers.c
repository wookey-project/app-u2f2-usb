#include "autoconf.h"
#include "libc/types.h"
#include "libc/sys/msg.h"
#include "libc/stdio.h"
#include "libc/errno.h"
#include "libc/nostd.h"
#include "libc/syscall.h"
#include "libc/string.h"

#include "handlers.h"
#include "main.h"

#define CONFIG_USR_APP_USB_DEBUG 0
#if CONFIG_USR_APP_USB_DEBUG
# define log_printf(...) printf(__VA_ARGS__)
#else
# define log_printf(...)
#endif


mbed_error_t handle_wink(uint16_t timeout_ms)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    int fido_msq = get_parser_msq();
    struct msgbuf msgbuf;
    size_t msgsz = 64;

    msgbuf.mtype = MAGIC_WINK_REQ;
    msgbuf.mtext.u16[0] = timeout_ms;

    /* syncrhonously send wink request */
    msgsnd(fido_msq, &msgbuf, 0, 0);
    /* and wait for response */
    msgrcv(fido_msq, &msgbuf, msgsz, MAGIC_ACKNOWLEDGE, 0);

    return errcode;
}


mbed_error_t u2fapdu_handle_cmd(uint32_t metadata, uint8_t *buf, uint16_t buf_len, uint8_t *resp, uint16_t *resp_len)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    int fido_msq = get_parser_msq();
    int ret;
    struct msgbuf msgbuf;
    size_t msgsz = 64; /* max msg buf size */

    /* request APDU CMD initialization to Fido backend */
    log_printf("[USB] sending APU to FIDO\n");
    //hexdump(buf, buf_len);

    log_printf("[USB] Send APDU_CMD_INIT to Fido\n");
    msgbuf.mtype = MAGIC_APDU_CMD_INIT;
    msgsnd(fido_msq, &msgbuf, 0, 0);

    log_printf("[USB] Send APDU_CMD_META to Fido : %x\n", metadata);
    msgbuf.mtype = MAGIC_APDU_CMD_META;
    msgbuf.mtext.u32[0] = metadata;
    msgsnd(fido_msq, &msgbuf, sizeof(uint32_t), 0);

    log_printf("[USB] Send APDU_CMD_MSG_LEN (len is %d) to Fido\n", buf_len);
    msgbuf.mtype = MAGIC_APDU_CMD_MSG_LEN;
    msgbuf.mtext.u16[0] = buf_len;
    msgsnd(fido_msq, &msgbuf, sizeof(uint32_t), 0);

    uint32_t num_full_msg = buf_len / 64;
    uint8_t residual_msg = buf_len % 64;
    uint32_t offset = 0;


    uint32_t i;
    for (i = 0; i < num_full_msg; ++i) {
        log_printf("[USB] Send APDU_CMD_MSG (pkt %d) to Fido\n", i);
        msgbuf.mtype = MAGIC_APDU_CMD_MSG;
        memcpy(&msgbuf.mtext.u8[0], &buf[offset], msgsz);
        msgsnd(fido_msq, &msgbuf, msgsz, 0);
        offset += msgsz;
    }
    if (residual_msg) {
        log_printf("[USB] Send APDU_CMD_MSG (pkt %d, residual, %d bytes) to Fido\n", i, residual_msg);
        msgbuf.mtype = MAGIC_APDU_CMD_MSG;
        memcpy(&msgbuf.mtext.u8[0], &buf[offset], residual_msg);
        msgsnd(fido_msq, &msgbuf, residual_msg, 0);
        offset += residual_msg;
    }
    /* APDU request fully send... */

    /* get back APDU response */
    msgrcv(fido_msq, &msgbuf, msgsz, MAGIC_APDU_RESP_INIT, 0);
    log_printf("[USB] received APDU_RESP_INIT from Fido\n");
    msgrcv(fido_msq, &msgbuf, msgsz, MAGIC_APDU_RESP_MSG_LEN, 0);
    log_printf("[USB] received APDU_RESP_MSG_LEN from Fido, %d bytes\n", msgbuf.mtext.u16[0]);

    /* FIXME: use u16 instead of u32 */
    *resp_len = msgbuf.mtext.u16[0];

    num_full_msg = *resp_len / 64;
    residual_msg = *resp_len % 64;
    offset = 0;

    for (i = 0; i < num_full_msg; ++i) {
        ret = msgrcv(fido_msq, &msgbuf, msgsz, MAGIC_APDU_RESP_MSG, 0);
        log_printf("[USB] received APDU_RESP_MSG (pkt %d) from Fido\n", i);
        memcpy(&resp[offset], &msgbuf.mtext.u8[0], msgsz);
        offset += msgsz;
    }
    if (residual_msg) {
        ret = msgrcv(fido_msq, &msgbuf, residual_msg, MAGIC_APDU_RESP_MSG, 0);
        log_printf("[USB] received APDU_RESP_MSG (pkt %d, residual, %d bytes) from Fido\n", i, ret);
        memcpy(&resp[offset], &msgbuf.mtext.u8[0], residual_msg);
        offset += residual_msg;
    }
    /* received overall APDU response from APDU/FIDO backend, get back return value */
    ret = msgrcv(fido_msq, &msgbuf, 1, MAGIC_CMD_RETURN, 0);

    errcode = msgbuf.mtext.u8[0];
    log_printf("[USB] received errcode %x from Fido\n", errcode);

    return errcode;
}
