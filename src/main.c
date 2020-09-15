#include "libc/syscall.h"
#include "libc/stdio.h"
#include "libc/time.h"
#include "libc/nostd.h"
#include "libc/string.h"
#include "libc/malloc.h"
#include "libc/regutils.h"
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

device_t    up;
device_t    button;
int    desc_up = 0;

void exti_button_handler (void)
{
    button_pushed = true;
}

static void wink_up(void)
{
    uint8_t ret;
    ret = sys_cfg(CFG_GPIO_SET, (uint8_t) up.gpios[0].kref.val, 1);
    if (ret != SYS_E_DONE) {
        printf ("sys_cfg(): failed\n");
    }
    ret = sys_cfg(CFG_GPIO_SET, (uint8_t) up.gpios[1].kref.val, 1);
    if (ret != SYS_E_DONE) {
        printf ("sys_cfg(): failed\n");
    }
}

static void wink_down(void)
{
    uint8_t ret;
    ret = sys_cfg(CFG_GPIO_SET, (uint8_t) up.gpios[0].kref.val, 0);
    if (ret != SYS_E_DONE) {
        printf ("sys_cfg(): failed\n");
    }
    ret = sys_cfg(CFG_GPIO_SET, (uint8_t) up.gpios[1].kref.val, 0);
    if (ret != SYS_E_DONE) {
        printf ("sys_cfg(): failed\n");
    }
}

static mbed_error_t handle_wink(uint16_t timeout_ms)
{
    wink_up();
    waitfor(timeout_ms);
    wink_down();

    return MBED_ERROR_NONE;
}


static mbed_error_t declare_userpresence_backend(void)
{
    uint8_t ret;
    /* Button + LEDs */
    memset (&up, 0, sizeof (up));

    strncpy (up.name, "UsPre", sizeof (up.name));
    up.gpio_num = 3; /* Number of configured GPIO */

    up.gpios[0].kref.port = led0_dev_infos.gpios[LED0_BASE].port;
    up.gpios[0].kref.pin = led0_dev_infos.gpios[LED0_BASE].pin;
    up.gpios[0].mask     = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD |
                                 GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
    up.gpios[0].mode     = GPIO_PIN_OUTPUT_MODE;
    up.gpios[0].pupd     = GPIO_PULLDOWN;
    up.gpios[0].type     = GPIO_PIN_OTYPER_PP;
    up.gpios[0].speed    = GPIO_PIN_HIGH_SPEED;


    up.gpios[1].kref.port = led1_dev_infos.gpios[LED0_BASE].port;
    up.gpios[1].kref.pin = led1_dev_infos.gpios[LED0_BASE].pin;
    up.gpios[1].mask     = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD |
                                 GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
    up.gpios[1].mode     = GPIO_PIN_OUTPUT_MODE;
    up.gpios[1].pupd     = GPIO_PULLDOWN;
    up.gpios[1].type     = GPIO_PIN_OTYPER_PP;
    up.gpios[1].speed    = GPIO_PIN_HIGH_SPEED;


    up.gpios[2].kref.port = dfu_button_dev_infos.gpios[DFU_BUTTON_BASE].port;
    up.gpios[2].kref.pin = dfu_button_dev_infos.gpios[DFU_BUTTON_BASE].pin;
    up.gpios[2].mask     = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD |
                                 GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED |
                                 GPIO_MASK_SET_EXTI;
    up.gpios[2].mode     = GPIO_PIN_INPUT_MODE;
    up.gpios[2].pupd     = GPIO_PULLDOWN;
    up.gpios[2].type     = GPIO_PIN_OTYPER_PP;
    up.gpios[2].speed    = GPIO_PIN_LOW_SPEED;
    up.gpios[2].exti_trigger = GPIO_EXTI_TRIGGER_RISE;
    up.gpios[2].exti_lock    = GPIO_EXTI_UNLOCKED;
    up.gpios[2].exti_handler = (user_handler_t) exti_button_handler;

    ret = sys_init(INIT_DEVACCESS, &up, &desc_up);
    if (ret == SYS_E_DONE) {
        return MBED_ERROR_NONE;
    }
    return MBED_ERROR_UNKNOWN;
}

void usbctrl_configuration_set(void)
{
    conf_set = true;
}


bool userpresence_backend(uint16_t timeout)
{
    /* wait half of duration and return ok by now */
    button_pushed = false;
    wink_up();
    printf("[USB] userpresence: waiting for %d ms\n", timeout/2);
    sys_sleep (timeout, SLEEP_MODE_INTERRUPTIBLE);
    if (button_pushed == true) {
        printf("[USB] button pushed !!!\n");
        wink_down();
        return true;
    }
    return false;
}


int _main(uint32_t task_id)
{
    task_id = task_id;
    char *wellcome_msg = "hello, I'm USB HID frontend";
    uint8_t ret;

    printf("%s\n", wellcome_msg);

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

    declare_userpresence_backend();


    ret = sys_init(INIT_DONE);
    if (ret != 0) {
        printf("failure while leaving init mode !!! err:%d\n", ret);
    }
    printf("sys_init DONE returns %x !\n", ret);

    /*
     * Let's declare a keyboard
     */
    //fido_declare(usbxdci_handler);

    /* FIXME: by now, directly pass U2FAPDU cmd handling (one task mechanism) */
    ctap_declare(usbxdci_handler, u2fapdu_handle_cmd, handle_wink);
    u2fapdu_register_callback(u2f_fido_handle_cmd);
    /* TODO callbacks protection */
    u2f_fido_initialize(userpresence_backend);



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

    printf("Going to error state!\n");
    aprintf_flush();
    return 1;
}
