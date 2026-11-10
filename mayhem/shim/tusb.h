/* Host stand-in for the Pico build's tusb.h: real TinyUSB HID types plus the few device/host calls
   the compiled deskhop sources make (implemented as inert stubs in shim.c). */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tusb_option.h"
#include "common/tusb_common.h"
#include "class/hid/hid.h"

bool tud_suspended(void);
void tud_remote_wakeup(void);
bool tud_hid_n_ready(uint8_t instance);
uint8_t tud_hid_n_get_protocol(uint8_t instance);
bool tud_hid_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len);
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]);
bool tud_mouse_report(uint8_t mode, uint8_t buttons, int16_t x, int16_t y, int8_t wheel, int8_t pan);
bool tuh_hid_set_report(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type,
                        void *report, uint16_t len);

/* Host-side state the harness controls. */
extern uint8_t mayhem_itf_protocol;
extern uint8_t mayhem_hid_protocol;

void tud_task(void);
bool tud_ready(void);
bool tuh_inited(void);
void tuh_task(void);
uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t idx);
uint8_t tuh_hid_get_protocol(uint8_t dev_addr, uint8_t idx);
bool tuh_hid_set_protocol(uint8_t dev_addr, uint8_t idx, uint8_t protocol);
bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t idx);
