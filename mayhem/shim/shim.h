#pragma once
#include <stdint.h>

/* Wipes all firmware state (as a fresh boot would) and sets the board role (OUTPUT_A / OUTPUT_B). */
void mayhem_reset_state(uint8_t board_role);

/* TinyUSB application callbacks implemented by src/usb.c (the headers only declare them when the
   stack is enabled). */
#include "tusb.h"
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize);
