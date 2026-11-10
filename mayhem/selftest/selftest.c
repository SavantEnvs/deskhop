/* Behavioral self-test for the deskhop firmware logic, run on a host through mayhem/shim.
   Each check feeds a known input through the real firmware code and asserts a known output
   (values come from the USB HID spec and the deskhop wire protocol, not from the code under test). */
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "shim.h"

static int passed, failed;

#define CHECK(name, cond)                                  \
    do {                                                   \
        if (cond) {                                        \
            passed++;                                      \
            printf("PASS %s\n", name);                     \
        } else {                                           \
            failed++;                                      \
            printf("FAIL %s (line %d)\n", name, __LINE__); \
        }                                                  \
    } while (0)

/* USB HID 1.11 appendix B.1 boot keyboard report descriptor. */
static const uint8_t kbd_desc[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01,
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0};

/* USB HID 1.11 appendix B.2 boot mouse report descriptor. */
static const uint8_t mouse_desc[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0xC0, 0xC0};

/* Boot mouse plus a wheel (Wheel usage 0x38) after the X/Y bytes, report IDs not used. */
static const uint8_t mouse_wheel_desc[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01, 0x29, 0x03,
    0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03,
    0x81, 0x06, 0xC0, 0xC0};


/* One interface carrying a keyboard (ID 1), a wheel mouse (ID 2), consumer control (ID 3) and
   system control (ID 4), the layout of typical "multimedia keyboard" and combo dongle devices. */
static const uint8_t composite_desc[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x75, 0x08, 0x95, 0x01, 0x81, 0x01, 0x95, 0x06,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x16, 0x01, 0x80, 0x26, 0xFF, 0x7F, 0x75, 0x10,
    0x95, 0x02, 0x81, 0x06, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
    0xC0, 0xC0,
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03, 0x15, 0x00, 0x26,
    0xFF, 0x03, 0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xC0,
    0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, 0x04, 0x19, 0x81, 0x29, 0x83, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x03, 0x81, 0x02, 0x75, 0x05, 0x95, 0x01, 0x81, 0x01, 0xC0};

static void feed_packet(uint8_t type, const uint8_t data[PACKET_DATA_LENGTH]) {
    uart_packet_t packet = {.type = type};
    memcpy(packet.data, data, PACKET_DATA_LENGTH);
    write_raw_packet(uart_rxbuf, &packet);

    global_state.dma_ptr = 0;
    mayhem_fake_dma[global_state.dma_rx_channel].transfer_count = DMA_RX_BUFFER_SIZE - RAW_PACKET_LENGTH;
    packet_receiver_task(&global_state);
}

static int pop_vendor_packet(uint8_t raw[RAW_PACKET_LENGTH]) {
    hid_generic_pkt_t pkt;
    if (!queue_try_remove(&global_state.hid_queue_out, &pkt))
        return 0;
    if (pkt.len != RAW_PACKET_LENGTH || pkt.report_id != REPORT_ID_VENDOR)
        return 0;
    memcpy(raw, pkt.data, RAW_PACKET_LENGTH);
    return 1;
}

static void test_checksum(void) {
    const uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    CHECK("checksum_is_xor_of_data", calc_checksum(data, 8) == 8);

    uart_packet_t packet = {.type = HEARTBEAT_MSG};
    memcpy(packet.data, data, 8);
    packet.checksum = 8;
    CHECK("checksum_accepts_matching", verify_checksum(&packet));
    packet.checksum = 9;
    CHECK("checksum_rejects_mismatch", !verify_checksum(&packet));
}

static void test_config_roundtrip(void) {
    mayhem_reset_state(OUTPUT_A);
    uint8_t raw[RAW_PACKET_LENGTH];

    /* GET of the config version returns the compiled-in version, little endian. */
    uint8_t get_version[8] = {70};
    feed_packet(GET_VAL_MSG, get_version);
    CHECK("get_version_response_queued", pop_vendor_packet(raw));
    CHECK("get_version_response_framing", raw[0] == START1 && raw[1] == START2);
    CHECK("get_version_response_type", raw[2] == GET_VAL_MSG && raw[3] == 70);
    uint32_t version = raw[4] | raw[5] << 8 | raw[6] << 16 | (uint32_t)raw[7] << 24;
    CHECK("get_version_value", version == CURRENT_CONFIG_VERSION);
    CHECK("get_version_response_checksum", raw[11] == calc_checksum(&raw[3], PACKET_DATA_LENGTH));

    /* SET of jump_threshold (field 77, UINT16) then GET returns what was written. */
    uint8_t set_threshold[8] = {77, 0x34, 0x12};
    feed_packet(SET_VAL_MSG, set_threshold);
    CHECK("set_value_stored", global_state.config.jump_threshold == 0x1234);

    uint8_t get_threshold[8] = {77};
    feed_packet(GET_VAL_MSG, get_threshold);
    CHECK("get_value_after_set", pop_vendor_packet(raw) && raw[3] == 77 && raw[4] == 0x34 && raw[5] == 0x12);

    /* Read-only fields (80: keyboard_connected) cannot be written. */
    uint8_t set_readonly[8] = {80, 1};
    feed_packet(SET_VAL_MSG, set_readonly);
    CHECK("readonly_field_not_written", !global_state.keyboard_connected);

    /* Unknown field indexes are ignored. */
    uint8_t get_unknown[8] = {200};
    feed_packet(GET_VAL_MSG, get_unknown);
    CHECK("unknown_field_gives_no_response", !pop_vendor_packet(raw));
}

static void test_bad_packets(void) {
    mayhem_reset_state(OUTPUT_A);

    /* A frame with a wrong checksum is dropped, the same frame with the right one is applied. */
    uart_packet_t packet = {.type = SET_VAL_MSG, .data = {77, 0x01, 0x02}};
    write_raw_packet(uart_rxbuf, &packet);
    uart_rxbuf[RAW_PACKET_LENGTH - 1] ^= 0xFF;
    global_state.dma_ptr = 0;
    mayhem_fake_dma[global_state.dma_rx_channel].transfer_count = DMA_RX_BUFFER_SIZE - RAW_PACKET_LENGTH;
    packet_receiver_task(&global_state);
    CHECK("bad_checksum_dropped", global_state.config.jump_threshold != 0x0201);

    uint8_t good[8] = {77, 0x01, 0x02};
    feed_packet(SET_VAL_MSG, good);
    CHECK("good_checksum_applied", global_state.config.jump_threshold == 0x0201);
}

static void test_vendor_channel(void) {
    mayhem_reset_state(OUTPUT_A);

    uart_packet_t packet = {.type = SET_VAL_MSG, .data = {77, 0x78, 0x56}};
    uint8_t raw[RAW_PACKET_LENGTH];
    write_raw_packet(raw, &packet);

    /* Outside config mode the vendor report is ignored. */
    tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, REPORT_ID_VENDOR, HID_REPORT_TYPE_OUTPUT, raw, sizeof(raw));
    CHECK("vendor_report_ignored_outside_config_mode", global_state.config.jump_threshold != 0x5678);

    global_state.config_mode_active = true;
    tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, REPORT_ID_VENDOR, HID_REPORT_TYPE_OUTPUT, raw, sizeof(raw) - 1);
    CHECK("vendor_report_wrong_size_ignored", global_state.config.jump_threshold != 0x5678);

    tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, REPORT_ID_VENDOR, HID_REPORT_TYPE_OUTPUT, raw, sizeof(raw));
    CHECK("vendor_report_applied_in_config_mode", global_state.config.jump_threshold == 0x5678);

    /* Packet types outside the allow-list (firmware transfer) are refused on this channel. */
    uart_packet_t fw = {.type = REQUEST_BYTE_MSG, .data = {0, 0, 0, 0}};
    write_raw_packet(raw, &fw);
    tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, REPORT_ID_VENDOR, HID_REPORT_TYPE_OUTPUT, raw, sizeof(raw));
    uart_packet_t queued;
    CHECK("vendor_channel_blocks_firmware_requests", !queue_try_remove(&global_state.uart_tx_queue, &queued));
}

static void test_request_byte(void) {
    mayhem_reset_state(OUTPUT_A);

    /* A firmware byte request is answered with RESPONSE_BYTE_MSG echoing the address. */
    uint8_t request[8] = {0x10, 0x00, 0x00, 0x00};
    feed_packet(REQUEST_BYTE_MSG, request);
    uart_packet_t response;
    CHECK("request_byte_answered", queue_try_remove(&global_state.uart_tx_queue, &response));
    CHECK("request_byte_response_type", response.type == RESPONSE_BYTE_MSG);
    CHECK("request_byte_response_address", response.data32[0] == 0x10);

    /* Addresses beyond the staging image are refused. */
    uint8_t beyond[8] = {0x00, 0x00, 0x10, 0x00};
    feed_packet(REQUEST_BYTE_MSG, beyond);
    CHECK("request_byte_out_of_range_ignored", !queue_try_remove(&global_state.uart_tx_queue, &response));
}

static void test_keyboard(void) {
    mayhem_reset_state(OUTPUT_A);
    mayhem_itf_protocol = HID_ITF_PROTOCOL_KEYBOARD;
    mayhem_hid_protocol = HID_PROTOCOL_BOOT;

    tuh_hid_mount_cb(1, 0, kbd_desc, sizeof(kbd_desc));
    CHECK("keyboard_marked_connected", global_state.keyboard_connected);
    CHECK("boot_keyboard_has_no_report_id", !global_state.iface[0][0].uses_report_id);

    /* Left Shift + 'a' (usage 0x04). */
    const uint8_t report[8] = {0x02, 0x00, 0x04, 0, 0, 0, 0, 0};
    tuh_hid_report_received_cb(1, 0, report, sizeof(report));

    hid_keyboard_report_t out;
    CHECK("keyboard_report_queued", queue_try_remove(&global_state.kbd_queue, &out));
    CHECK("keyboard_modifier_forwarded", out.modifier == 0x02);
    CHECK("keyboard_keycode_forwarded", out.keycode[0] == 0x04 && out.keycode[1] == 0);

    /* Release. */
    const uint8_t release[8] = {0};
    tuh_hid_report_received_cb(1, 0, release, sizeof(release));
    CHECK("keyboard_release_queued", queue_try_remove(&global_state.kbd_queue, &out));
    CHECK("keyboard_release_empty", out.modifier == 0 && out.keycode[0] == 0);

    /* A short report is ignored. */
    tuh_hid_report_received_cb(1, 0, report, 4);
    CHECK("short_keyboard_report_ignored", !queue_try_remove(&global_state.kbd_queue, &out));

    tuh_hid_umount_cb(1, 0);
    CHECK("keyboard_marked_disconnected", !global_state.keyboard_connected);
}

static void test_mouse(void) {
    mayhem_reset_state(OUTPUT_A);
    mayhem_itf_protocol = HID_ITF_PROTOCOL_MOUSE;
    mayhem_hid_protocol = HID_PROTOCOL_REPORT;

    tuh_hid_mount_cb(1, 0, mouse_wheel_desc, sizeof(mouse_wheel_desc));
    CHECK("mouse_marked_connected", global_state.mouse_connected);
    CHECK("mouse_found_in_descriptor", global_state.iface[0][0].mouse.is_found);

    /* Left button held, X +5, Y -3, wheel +1. */
    const uint8_t report[4] = {0x01, 5, 0xFD, 1};
    tuh_hid_report_received_cb(1, 0, report, sizeof(report));
    CHECK("mouse_buttons_tracked", global_state.mouse_buttons == 1);
}

static void test_composite(void) {
    mayhem_reset_state(OUTPUT_A);
    mayhem_itf_protocol = HID_ITF_PROTOCOL_NONE;
    mayhem_hid_protocol = HID_PROTOCOL_REPORT;

    tuh_hid_mount_cb(1, 0, composite_desc, sizeof(composite_desc));
    hid_interface_t *iface = &global_state.iface[0][0];
    CHECK("composite_uses_report_ids", iface->uses_report_id);
    CHECK("composite_mouse_found", iface->mouse.is_found);
    CHECK("composite_mouse_connected", global_state.mouse_connected);

    /* Keyboard, report ID 1: Left Ctrl + Enter (usage 0x28). */
    const uint8_t kbd[9] = {0x01, 0x01, 0x00, 0x28, 0, 0, 0, 0, 0};
    tuh_hid_report_received_cb(1, 0, kbd, sizeof(kbd));
    hid_keyboard_report_t out;
    CHECK("composite_keyboard_queued", queue_try_remove(&global_state.kbd_queue, &out));
    CHECK("composite_keyboard_values", out.modifier == 0x01 && out.keycode[0] == 0x28);

    /* Consumer control, report ID 3: Volume Increment (usage 0x00E9), little endian. */
    const uint8_t consumer[3] = {0x03, 0xE9, 0x00};
    tuh_hid_report_received_cb(1, 0, consumer, sizeof(consumer));
    hid_generic_pkt_t pkt;
    CHECK("consumer_report_queued", queue_try_remove(&global_state.hid_queue_out, &pkt));
    CHECK("consumer_report_routing", pkt.report_id == REPORT_ID_CONSUMER && pkt.len == CONSUMER_CONTROL_LENGTH);
    CHECK("consumer_report_usage", pkt.data[0] == 0xE9 && pkt.data[1] == 0x00);

    /* System control, report ID 4: bit 0 = Power Down. */
    const uint8_t system[2] = {0x04, 0x01};
    tuh_hid_report_received_cb(1, 0, system, sizeof(system));
    CHECK("system_report_queued", queue_try_remove(&global_state.hid_queue_out, &pkt));
    CHECK("system_report_routing", pkt.report_id == REPORT_ID_SYSTEM && pkt.data[0] == 0x01);

    /* Mouse, report ID 2: left button, X +5, Y -3 (16-bit), wheel +1. */
    const uint8_t mouse[7] = {0x02, 0x01, 0x05, 0x00, 0xFD, 0xFF, 0x01};
    tuh_hid_report_received_cb(1, 0, mouse, sizeof(mouse));
    mouse_report_t m;
    CHECK("composite_mouse_queued", queue_try_remove(&global_state.mouse_queue, &m));
    CHECK("composite_mouse_buttons_wheel", m.buttons == 0x01 && m.wheel == 1);
    CHECK("composite_mouse_buttons_state", global_state.mouse_buttons == 0x01);
}

int main(void) {
    test_checksum();
    test_config_roundtrip();
    test_bad_packets();
    test_vendor_channel();
    test_request_byte();
    test_keyboard();
    test_mouse();
    test_composite();

    printf("SUMMARY passed=%d failed=%d\n", passed, failed);
    return failed ? 1 : 0;
}
