/* Fuzzes deskhop's USB-host HID path: report descriptor parsing (tuh_hid_mount_cb) followed by
   interpretation of input reports (tuh_hid_report_received_cb) for the parsed layout.

   Input layout:
     byte 0   bits 0-1 interface protocol (0 none, 1 keyboard, 2 mouse; 3 is rejected)
              bit  2   HID protocol (0 boot, 1 report)
              bit  3   board role (OUTPUT_A / OUTPUT_B)
              bits 4-5 device address - 1
     byte 1   instance (0..MAX_INTERFACES-1)
     bytes 2-3 report descriptor length (LE), at most 512 and within the remaining bytes
     then     the descriptor, followed by input reports, each as one length byte (at most 64) + payload.

   TinyUSB hands the application the descriptor in its 512-byte enumeration buffer and each input report
   in a 64-byte endpoint buffer (CFG_TUH_ENUMERATION_BUFSIZE, CFG_TUH_HID_EPIN_BUFSIZE), and never
   delivers more than fit; the harness delivers them the same way. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "shim.h"

#define ENUM_BUF_SIZE 512
#define EPIN_BUF_SIZE 64

static void drain_queues(void) {
    uint8_t scratch[64];
    queue_t *queues[] = {&global_state.kbd_queue, &global_state.mouse_queue,
                         &global_state.hid_queue_out, &global_state.uart_tx_queue};
    for (unsigned i = 0; i < 4; i++)
        while (queue_try_remove(queues[i], scratch))
            ;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4)
        return -1;

    uint8_t itf_protocol = data[0] & 3;
    if (itf_protocol == 3)
        return -1;
    uint8_t hid_protocol = (data[0] >> 2) & 1;
    uint8_t board_role   = (data[0] >> 3) & 1;
    uint8_t dev_addr     = 1 + ((data[0] >> 4) & 3);
    uint8_t instance     = data[1];
    size_t desc_len      = (size_t)data[2] | ((size_t)data[3] << 8);

    if (instance >= MAX_INTERFACES)
        return -1;

    const uint8_t *rest = data + 4;
    size_t rest_len     = size - 4;
    if (desc_len > rest_len || desc_len > ENUM_BUF_SIZE)
        return -1;

    uint8_t *desc = calloc(1, ENUM_BUF_SIZE);
    memcpy(desc, rest, desc_len);
    rest += desc_len;
    rest_len -= desc_len;

    mayhem_reset_state(board_role);
    mayhem_itf_protocol = itf_protocol;
    mayhem_hid_protocol = hid_protocol ? HID_PROTOCOL_REPORT : HID_PROTOCOL_BOOT;

    tuh_hid_mount_cb(dev_addr, instance, desc, (uint16_t)desc_len);
    drain_queues();

    for (int n = 0; n < 64 && rest_len > 0; n++) {
        size_t len = rest[0];
        rest++;
        rest_len--;
        if (len > rest_len)
            len = rest_len;
        if (len > EPIN_BUF_SIZE) {
            free(desc);
            return -1;
        }

        uint8_t *report = calloc(1, EPIN_BUF_SIZE);
        memcpy(report, rest, len);
        rest += len;
        rest_len -= len;

        tuh_hid_report_received_cb(dev_addr, instance, report, (uint16_t)len);
        free(report);
        drain_queues();
    }

    tuh_hid_umount_cb(dev_addr, instance);
    free(desc);
    return 0;
}
