/* Fuzzes deskhop's board-to-board UART protocol and the USB vendor configuration channel: both end
   in process_packet() and its handler table (config get/set, mode switches, firmware transfer...).

   Input layout:
     byte 0  bits 0-1 mode:
               0  records (type + 8 data bytes each) framed by the firmware's own write_raw_packet and
                  fed through the DMA ring receive task
               1  raw bytes placed straight into the DMA ring (preamble scan, checksum check)
               2  one framed record delivered via the USB vendor set_report callback
               3  raw bytes delivered via the USB vendor set_report callback
             bit 2   config mode active (vendor channel only)
             bit 3   board role (OUTPUT_A / OUTPUT_B)
     modes 0 and 2: the rest are 9-byte records
     mode 3:        byte 1 report id, byte 2 report type, rest is the report (at most 64 bytes, the size of
                    TinyUSB's control transfer buffer, which is what set_report callbacks point into) */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "shim.h"

#define CTRL_BUF_SIZE 64
#define RECORD_LEN (1 + PACKET_DATA_LENGTH)

static void drain_queues(void) {
    uint8_t scratch[64];
    queue_t *queues[] = {&global_state.kbd_queue, &global_state.mouse_queue,
                         &global_state.hid_queue_out, &global_state.uart_tx_queue};
    for (unsigned i = 0; i < 4; i++)
        while (queue_try_remove(queues[i], scratch))
            ;
}

static void receive_from_ring(size_t len) {
    mayhem_fake_dma[global_state.dma_rx_channel].transfer_count = DMA_RX_BUFFER_SIZE - len;
    for (int i = 0; i < 128; i++) {
        packet_receiver_task(&global_state);
        drain_queues();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1)
        return -1;

    uint8_t mode        = data[0] & 3;
    bool config_mode    = (data[0] >> 2) & 1;
    uint8_t board_role  = (data[0] >> 3) & 1;
    const uint8_t *rest = data + 1;
    size_t rest_len     = size - 1;

    mayhem_reset_state(board_role);
    global_state.config_mode_active = config_mode;

    switch (mode) {
        case 0: {
            size_t count = rest_len / RECORD_LEN;
            if (count == 0 || count * RAW_PACKET_LENGTH > DMA_RX_BUFFER_SIZE - 1)
                return -1;

            for (size_t i = 0; i < count; i++) {
                uart_packet_t packet = {.type = rest[i * RECORD_LEN]};
                memcpy(packet.data, &rest[i * RECORD_LEN + 1], PACKET_DATA_LENGTH);
                write_raw_packet(&uart_rxbuf[i * RAW_PACKET_LENGTH], &packet);
            }
            receive_from_ring(count * RAW_PACKET_LENGTH);
            break;
        }
        case 1:
            if (rest_len > DMA_RX_BUFFER_SIZE - 1)
                return -1;
            memcpy(uart_rxbuf, rest, rest_len);
            receive_from_ring(rest_len);
            break;
        case 2: {
            if (rest_len != RECORD_LEN)
                return -1;
            uart_packet_t packet = {.type = rest[0]};
            memcpy(packet.data, &rest[1], PACKET_DATA_LENGTH);

            uint8_t *buffer = calloc(1, CTRL_BUF_SIZE);
            write_raw_packet(buffer, &packet);
            tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, REPORT_ID_VENDOR, HID_REPORT_TYPE_OUTPUT, buffer,
                                  RAW_PACKET_LENGTH);
            free(buffer);
            drain_queues();
            break;
        }
        default: {
            if (rest_len < 2)
                return -1;
            uint8_t report_id   = rest[0];
            uint8_t report_type = rest[1];
            size_t len          = rest_len - 2;
            if (len > CTRL_BUF_SIZE)
                return -1;

            uint8_t *buffer = calloc(1, CTRL_BUF_SIZE);
            memcpy(buffer, rest + 2, len);
            tud_hid_set_report_cb(ITF_NUM_HID_VENDOR, report_id, (hid_report_type_t)report_type, buffer,
                                  (uint16_t)len);
            free(buffer);
            drain_queues();
            break;
        }
    }
    return 0;
}
