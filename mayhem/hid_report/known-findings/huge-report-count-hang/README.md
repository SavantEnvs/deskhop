# Huge HID Report Count hangs the descriptor parser

**Input:** `input.bin` (a HID report descriptor declaring an Input item with Report Count `0x7FFFFFFF`).

**Cause:** `handle_main_input()` in `src/hid_parser.c` loops over `report_count` calling `get_usage()` once per
field with no upper bound, so a device descriptor claiming ~2^31 fields keeps the CPU busy for minutes.

**Impact:** a malicious or malformed USB HID device that is plugged into the keyboard/mouse port makes
`tuh_hid_mount_cb()` spin, which freezes the KVM firmware (denial of service; the watchdog is only fed from the
main loop).

**Upstream fix (one line):** reject or clamp the descriptor in `handle_main_input()`, e.g.
`if (report_count > MAX_REPORT_COUNT) return;` before the per-field loop.

Reproduce: `timeout 10 /mayhem/hid_report-standalone mayhem/hid_report/known-findings/huge-report-count-hang/input.bin`
(exit 124).
