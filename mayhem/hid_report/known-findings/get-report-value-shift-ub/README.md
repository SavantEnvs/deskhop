# `get_report_value()` shifts by a descriptor-controlled field size

**Inputs:**
- `input.bin`: a descriptor whose field Report Size is 257 bits, so `src/hid_report.c:29` runs
  `1u << 257` ("shift exponent 257 is too large for 32-bit type 'unsigned int'").
- `input-sign-bit.bin`: a field wide enough that `src/hid_report.c:36` runs `report[++byte_offset] << 24`
  with a byte >= 0x80 ("left shift of 255 by 24 places cannot be represented in type 'int'").

**Cause:** one root cause. `report_val_t.size` is copied unchecked from the device's Report Size item
(`uint16_t`, up to 65535), and `get_report_value()` assumes `size < 32`. Line 29 (`1u << val->size`) is
undefined for `size >= 32`. Line 36 (`report[++byte_offset] << remaining_bits`) is undefined once
`remaining_bits` reaches 24 (a byte shifted into the sign bit of `int`), which happens for fields wider than
24 bits. The sign-extension shift `0xFFFFFFFFU << val->size` on line 41 has the same flaw. The 5 UBSan
defects Mayhem reports on `hid-report` for `get_report_value` (4 at `:29`, 1 at `:36`) are this one bug.

**Impact:** a malicious or malformed USB HID device can make the firmware compute a wrong mask and wrong
field values (and, with compiler-dependent shift semantics, an arbitrary mask) for the keyboard/mouse
fields, which feed the pointer and key state. On the RP2040 the shifts do not trap, so the effect is
corrupted input values, not a crash.

**Upstream fix:** reject or clamp `size > 32` when the field is parsed, and do the accumulation in
`uint32_t`/`uint64_t`, e.g. `uint32_t mask = (val->size >= 32) ? 0xFFFFFFFFu : ((1u << val->size) - 1);`
and `result |= (uint32_t)report[++byte_offset] << remaining_bits;`.

Reproduce: `/mayhem/hid_report-standalone mayhem/hid_report/known-findings/get-report-value-shift-ub/input.bin`
(UBSan abort at `hid_report.c:29`); same with `input-sign-bit.bin` (abort at `hid_report.c:36`).
