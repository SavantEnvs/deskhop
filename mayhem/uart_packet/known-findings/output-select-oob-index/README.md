# Unvalidated OUTPUT_SELECT value indexes `config.output[]` out of bounds

**Input:** `input.bin` (two UART packets: `OUTPUT_SELECT_MSG` with value `0x58`, then `SYNC_BORDERS_MSG`).

**Cause:** `handle_output_select_msg()` in `src/handlers.c` stores the received byte in
`global_state->active_output` without range checking; `handle_sync_borders_msg()` (`src/handlers.c:231`) then
uses it as an index into `config.output[NUM_SCREENS]`.

**Impact:** whoever can write to the inter-board UART link (or a compromised peer board) makes the receiving board
read, and via later writes possibly modify, memory past the `output[]` array (UBSan: index out of bounds; on
hardware an out-of-bounds read/write in firmware).

**Upstream fix (one line):** in `handle_output_select_msg()`, ignore values `>= NUM_SCREENS`:
`if (msg->data[0] >= NUM_SCREENS) return;`.

Reproduce: `/mayhem/uart_packet-standalone mayhem/uart_packet/known-findings/output-select-oob-index/input.bin`.
