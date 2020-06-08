#include "libc/syscall.h"
#include "libc/stdio.h"
#include "libc/time.h"
#include "libc/nostd.h"
#include "libc/string.h"
#include "libc/malloc.h"
#include "libc/regutils.h"
#include "autoconf.h"
#include "libusbctrl.h"
#include "libkeyboard.h"
#include "generated/devlist.h"
#include "main.h"



/* libusbctrl specific triggers and contexts */

/* let's declare keyboard collection
 * We declare all keyboard generic keys in first collection and multimedia keys in
 * second collection
 */

/* first report (identified by report index 0) */

volatile bool reset_requested = false;

uint32_t usbxdci_handler;

void usbctrl_reset_received(void) {
    reset_requested = true;
}

static volatile bool conf_set = false;

void usbctrl_configuration_set(void)
{
    conf_set = true;
}


int _main(uint32_t task_id)
{
    task_id = task_id;
    char *wellcome_msg = "hello, I'm USB HID frontend";
    uint8_t ret;

    printf("%s\n", wellcome_msg);

    /* initialize USB Control plane */
#if CONFIG_APP_USBHID_USR_DRV_USB_HS
    printf("declare usbctrl context for HS\n");
    usbctrl_declare(USB_OTG_HS_ID, &usbxdci_handler);
#elif CONFIG_APP_USBHID_USR_DRV_USB_FS
    printf("declare usbctrl context for FS\n");
    usbctrl_declare(USB_OTG_FS_ID, &usbxdci_handler);
#else
# error "Unsupported USB driver backend"
#endif
    printf("initialize usbctrl with handler %d\n", usbxdci_handler);
    usbctrl_initialize(usbxdci_handler);


    ret = sys_init(INIT_DONE);
    if (ret != 0) {
        printf("failure while leaving init mode !!! err:%d\n", ret);
    }
    printf("sys_init DONE returns %x !\n", ret);

    /*
     * Let's declare a keyboard
     */
    wmalloc_init();
    keyboard_declare(usbxdci_handler);

    /*******************************************
     * End of init sequence, let's initialize devices
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
    printf("Set configuration received\n");

    /* let's talk :-) */
    /* init report with empty content */
    do {
        while (!reset_requested) {
            //hid_exec_automaton();
            keyboard_exec();
        }
    } while (1);

err:
    printf("Going to error state!\n");
    aprintf_flush();
    return 1;
}
