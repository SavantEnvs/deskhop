/* Host stand-ins for the RP2040 hardware / pico-sdk / TinyUSB calls the deskhop sources make.
   Everything here is inert: no I/O, deterministic clock, bounded FIFO queues. */
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "pico/bootrom.h"
#include "hardware/dma.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "tusb.h"
#include "main.h"
#include "shim.h"

device_t global_state = {0};
firmware_metadata_t _firmware_metadata = {.version = 0x0001};

uint8_t mayhem_fake_ppb[0x20000];
ioqspi_hw_t mayhem_fake_ioqspi;
sio_hw_t mayhem_fake_sio;
watchdog_hw_t mayhem_fake_watchdog;
dma_channel_hw_t mayhem_fake_dma[16];

uint8_t mayhem_itf_protocol;
uint8_t mayhem_hid_protocol;

uint8_t uart_rxbuf[DMA_RX_BUFFER_SIZE] __attribute__((aligned(DMA_RX_BUFFER_SIZE)));
uint8_t uart_txbuf[DMA_TX_BUFFER_SIZE] __attribute__((aligned(DMA_TX_BUFFER_SIZE)));

/* Flash images the firmware reads through XIP. The tail lets the word read at the last valid
   offset stay inside the array. */
const config_t ADDR_CONFIG[1] = {{0}};
const uint8_t ADDR_FW_METADATA[FLASH_PAGE_SIZE] = {0};
const uint8_t ADDR_FW_RUNNING[STAGING_IMAGE_SIZE + 16] = {0};
const uint8_t ADDR_FW_STAGING[STAGING_IMAGE_SIZE] = {0};
const uint8_t ADDR_DISK_IMAGE[4096] = {0};

/* ---------- state reset ---------- */

static queue_t *const all_queues[] = {&global_state.kbd_queue, &global_state.mouse_queue,
                                      &global_state.hid_queue_out, &global_state.uart_tx_queue};

static uint64_t fake_clock_us;

void mayhem_reset_state(uint8_t board_role) {
    for (unsigned i = 0; i < sizeof(all_queues) / sizeof(all_queues[0]); i++)
        queue_free(all_queues[i]);

    memset(&global_state, 0, sizeof(global_state));
    memset(mayhem_fake_ppb, 0, sizeof(mayhem_fake_ppb));
    memset(&mayhem_fake_watchdog, 0, sizeof(mayhem_fake_watchdog));
    memset(mayhem_fake_dma, 0, sizeof(mayhem_fake_dma));
    memset(uart_rxbuf, 0, sizeof(uart_rxbuf));
    memset(uart_txbuf, 0, sizeof(uart_txbuf));
    mayhem_itf_protocol = 0;
    mayhem_hid_protocol = 0;
    fake_clock_us       = 1000000;

    global_state.board_role    = board_role;
    global_state.tud_connected = true; /* the host PC has enumerated us */
    load_config(&global_state);

    queue_init(&global_state.kbd_queue, sizeof(hid_keyboard_report_t), KBD_QUEUE_LENGTH);
    queue_init(&global_state.mouse_queue, sizeof(mouse_report_t), MOUSE_QUEUE_LENGTH);
    queue_init(&global_state.hid_queue_out, sizeof(hid_generic_pkt_t), HID_QUEUE_LENGTH);
    queue_init(&global_state.uart_tx_queue, sizeof(uart_packet_t), UART_QUEUE_LENGTH);
}

/* ---------- queue ---------- */

void queue_init(queue_t *q, uint element_size, uint element_count) {
    q->data         = calloc(element_count, element_size);
    q->element_size = element_size;
    q->element_count = element_count;
    q->head         = 0;
    q->used         = 0;
}

void queue_free(queue_t *q) {
    free(q->data);
    memset(q, 0, sizeof(*q));
}

bool queue_try_add(queue_t *q, const void *data) {
    if (q->used >= q->element_count)
        return false;
    uint32_t tail = (q->head + q->used) % q->element_count;
    memcpy(q->data + (size_t)tail * q->element_size, data, q->element_size);
    q->used++;
    return true;
}

bool queue_try_peek(queue_t *q, void *data) {
    if (q->used == 0)
        return false;
    memcpy(data, q->data + (size_t)q->head * q->element_size, q->element_size);
    return true;
}

bool queue_try_remove(queue_t *q, void *data) {
    if (!queue_try_peek(q, data))
        return false;
    q->head = (q->head + 1) % q->element_count;
    q->used--;
    return true;
}

bool queue_is_empty(queue_t *q) { return q->used == 0; }
bool queue_is_full(queue_t *q) { return q->used >= q->element_count; }

/* ---------- time, gpio, interrupts, flash, dma ---------- */

uint64_t time_us_64(void) { return fake_clock_us += 100; }
uint32_t time_us_32(void) { return (uint32_t)time_us_64(); }
void sleep_us(uint64_t us) { fake_clock_us += us; }
void sleep_ms(uint32_t ms) { fake_clock_us += (uint64_t)ms * 1000; }

void gpio_init(uint gpio) { (void)gpio; }
void gpio_set_dir(uint gpio, bool out) { (void)gpio; (void)out; }
void gpio_put(uint gpio, bool value) { (void)gpio; (void)value; }
bool gpio_get(uint gpio) { (void)gpio; return false; }

uint32_t save_and_disable_interrupts(void) { return 0; }
void restore_interrupts(uint32_t status) { (void)status; }
void watchdog_update(void) {}

void flash_range_erase(uint32_t flash_offs, size_t count) { (void)flash_offs; (void)count; }
void flash_range_program(uint32_t flash_offs, const uint8_t *data, size_t count) {
    (void)flash_offs; (void)data; (void)count;
}
void reset_usb_boot(uint32_t usb_activity_gpio_pin_mask, uint32_t disable_interface_mask) {
    (void)usb_activity_gpio_pin_mask; (void)disable_interface_mask;
}

bool dma_channel_is_busy(uint32_t channel) { (void)channel; return false; }
void dma_channel_transfer_from_buffer_now(uint32_t channel, const void *buf, uint32_t count) {
    (void)channel; (void)buf; (void)count;
}

/* ---------- TinyUSB device / host ---------- */

bool tud_suspended(void) { return false; }
void tud_remote_wakeup(void) {}
bool tud_ready(void) { return true; }
void tud_task(void) {}
bool tud_hid_n_ready(uint8_t instance) { (void)instance; return true; }
uint8_t tud_hid_n_get_protocol(uint8_t instance) { (void)instance; return HID_PROTOCOL_REPORT; }
bool tud_hid_n_report(uint8_t instance, uint8_t report_id, void const *report, uint16_t len) {
    (void)instance; (void)report_id; (void)report; (void)len;
    return true;
}
bool tud_hid_keyboard_report(uint8_t report_id, uint8_t modifier, const uint8_t keycode[6]) {
    (void)report_id; (void)modifier; (void)keycode;
    return true;
}
bool tud_mouse_report(uint8_t mode, uint8_t buttons, int16_t x, int16_t y, int8_t wheel, int8_t pan) {
    (void)mode; (void)buttons; (void)x; (void)y; (void)wheel; (void)pan;
    return true;
}

bool tuh_inited(void) { return true; }
void tuh_task(void) {}
uint8_t tuh_hid_interface_protocol(uint8_t dev_addr, uint8_t idx) {
    (void)dev_addr; (void)idx;
    return mayhem_itf_protocol;
}
uint8_t tuh_hid_get_protocol(uint8_t dev_addr, uint8_t idx) {
    (void)dev_addr; (void)idx;
    return mayhem_hid_protocol;
}
bool tuh_hid_set_protocol(uint8_t dev_addr, uint8_t idx, uint8_t protocol) {
    (void)dev_addr; (void)idx;
    mayhem_hid_protocol = protocol;
    return true;
}
bool tuh_hid_receive_report(uint8_t dev_addr, uint8_t idx) { (void)dev_addr; (void)idx; return true; }
bool tuh_hid_set_report(uint8_t dev_addr, uint8_t idx, uint8_t report_id, uint8_t report_type,
                        void *report, uint16_t len) {
    (void)dev_addr; (void)idx; (void)report_id; (void)report_type; (void)report; (void)len;
    return true;
}
