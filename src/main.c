#include "libc/syscall.h"
#include "libc/stdio.h"
#include "libc/time.h"
#include "libc/nostd.h"
#include "libc/string.h"
#include "libc/malloc.h"
#include "libc/regutils.h"
#include "libc/sys/msg.h"
#include "libc/errno.h"
#include "libu2f2.h"
#include "autoconf.h"
#include "libusbctrl.h"
#include "libusbhid.h"
#include "libu2fapdu.h"
#include "libfido.h"
#include "libctap.h"
#include "generated/devlist.h"
#include "generated/led0.h"
#include "generated/led1.h"
#include "generated/dfu_button.h"
#include "main.h"
#include "handlers.h"


/* libusbctrl specific triggers and contexts */

/* let's declare keyboard collection
 * We declare all keyboard generic keys in first collection and multimedia keys in
 * second collection
 */

/* first report (identified by report index 0) */

volatile bool reset_requested = false;
volatile bool button_pushed = false;

uint32_t usbxdci_handler;

void usbctrl_reset_received(void) {
    reset_requested = true;
}

static volatile bool conf_set = false;

int    desc_up = 0;

void usbctrl_configuration_set(void)
{
    conf_set = true;
}


int parser_msq = 0;

int get_parser_msq(void) {
    return parser_msq;
}

int _main(uint32_t task_id)
{
    task_id = task_id;
    char *wellcome_msg = "hello, I'm USB HID frontend";
    uint8_t ret;

    printf("%s\n", wellcome_msg);
    wmalloc_init();

    /* initialize USB Control plane */
#if CONFIG_APP_USB_USR_DRV_USB_HS
    printf("declare usbctrl context for HS\n");
    usbctrl_declare(USB_OTG_HS_ID, &usbxdci_handler);
#elif CONFIG_APP_USB_USR_DRV_USB_FS
    printf("declare usbctrl context for FS\n");
    usbctrl_declare(USB_OTG_FS_ID, &usbxdci_handler);
#else
# error "Unsupported USB driver backend"
#endif
    printf("initialize usbctrl with handler %d\n", usbxdci_handler);
    usbctrl_initialize(usbxdci_handler);

    printf("initialize Posix SystemV message queue with parser task\n");
    parser_msq = msgget("parser", IPC_CREAT | IPC_EXCL);
    if (parser_msq == -1) {
        printf("error while requesting SysV message queue. Errno=%x\n", errno);
        goto err;
    }
    ret = sys_init(INIT_DONE);
    if (ret != 0) {
        printf("failure while leaving init mode !!! err:%d\n", ret);
        goto err;
    }
    printf("sys_init DONE returns %x !\n", ret);

    /*
     * Let's declare a keyboard
     */
    ctap_declare(usbxdci_handler, u2fapdu_handle_cmd, handle_wink);

    /*******************************************
     * End of init sequence, let's wait for backend
     *******************************************/


    if (send_signal_with_acknowledge(parser_msq, MAGIC_IS_BACKEND_READY, MAGIC_BACKEND_IS_READY) != MBED_ERROR_NONE) {
        printf("failed while requesting PIN for confirm unlock! erro=%d\n", errno);
        goto err;
    }

    /* ==> user has unlock the platform, backend is fully ready */

    /*******************************************
     * End of auth, let's initialize USB
     *******************************************/

    /* Start USB device */
    printf("start usbhid device\n");
    usbctrl_start_device(usbxdci_handler);

    /*******************************************
     * Starting USB listener
     *******************************************/

    printf("USB main loop starting\n");
    /* wait for nego, 1s */

    /* wait for set_configuration trigger... */
    reset_requested = false;
    /* in case of RESET, reinit context to empty values */
    //usbhid_reinit();
    /* wait for SetConfiguration */
    while (!conf_set) {
        aprintf_flush();
    }
    /* TODO: reimplement malloc properly ! */
    wmalloc_init();

    ctap_configure();
    printf("Set configuration received\n");
    /* let's talk :-) */
    /* init report with empty content */
    do {
        while (!reset_requested) {
            //hid_exec_automaton();
            ctap_exec();
        }
    } while (1);

err:
    printf("Going to error state!\n");
    return 1;
}
