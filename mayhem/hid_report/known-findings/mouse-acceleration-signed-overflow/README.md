# Signed overflow in `calculate_mouse_acceleration_factor()`

**Input:** `input.bin` (a mouse descriptor with 32-bit X/Y fields, plus a report with large deltas).

**Cause:** `src/mouse.c:92` computes `(float)(offset_x * offset_x)` and `(float)(offset_y * offset_y)` with the
multiplication done in `int32_t`, before the cast to float. The deltas come from `get_report_value()`, which
returns up to 32 significant bits, so `|offset| > 46340` overflows ("signed integer overflow: -114272 *
-114272 cannot be represented in type 'int32_t'"). This one is independent of the `get_report_value()` shift
bug: the product overflows even for a well-formed 32-bit field.

**Impact:** a device (or a mouse with a wide delta field and a large movement) makes the magnitude, and
therefore the acceleration factor, wrap to a wrong value (possibly negative, giving `sqrtf` of a negative
number, i.e. NaN, which falls into the `return 1.0` fallback). The effect is a wrong pointer step for that
report, not memory corruption.

**Upstream fix (one line):** multiply in float, `sqrtf((float)offset_x * (float)offset_x + (float)offset_y *
(float)offset_y)`, or use `hypotf((float)offset_x, (float)offset_y)`.

Reproduce: `/mayhem/hid_report-standalone mayhem/hid_report/known-findings/mouse-acceleration-signed-overflow/input.bin`
(UBSan abort at `mouse.c:92`).
