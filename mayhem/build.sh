#!/usr/bin/env bash
#
# Builds deskhop's fuzz harnesses and its behavioral self-test on a host.
#
# deskhop is RP2040 firmware: the Pico SDK and the hardware are not available here, so the firmware
# sources under src/ are compiled unmodified against mayhem/shim/ (inert stand-ins for the SDK, hardware
# and TinyUSB device/host calls; the real TinyUSB headers in pico-sdk/lib/tinyusb supply the HID types).
# The harnesses drive the firmware's USB-host HID path (descriptor parsing and report handling) and its
# UART / vendor-config packet path.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE COVERAGE_FLAGS

cd "$SRC"

FIRMWARE="usb tasks hid_parser hid_report keyboard mouse utils uart protocol handlers constants defaults led"
TARGETS="hid_report uart_packet"
INCLUDES="-I$SRC/mayhem/shim -I$SRC/pico-sdk/lib/tinyusb/src -I$SRC/src/include"
WARN="-w"

BUILD="$SRC/build"
rm -rf "$BUILD"
mkdir -p "$BUILD/fuzz" "$BUILD/test"

# 1) Firmware + shim, sanitized, with SanCov instrumentation (-fsanitize=fuzzer-no-link) and DWARF 3.
for f in $FIRMWARE; do
    $CC -std=gnu11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link $WARN $INCLUDES \
        -c "$SRC/src/$f.c" -o "$BUILD/fuzz/$f.o"
done
$CC -std=gnu11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link $WARN $INCLUDES \
    -c "$SRC/mayhem/shim/shim.c" -o "$BUILD/fuzz/shim.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$BUILD/fuzz/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$BUILD/fuzz/standalone_main.o"

# 2) One libFuzzer binary and one standalone reproducer per harness.
for t in $TARGETS; do
    $CC -std=gnu11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link $WARN $INCLUDES \
        -c "$SRC/mayhem/$t/harness.c" -o "$BUILD/fuzz/harness_$t.o"
    objs=$(ls "$BUILD"/fuzz/*.o | grep -v -e '/harness_' -e '/standalone_main.o')
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$BUILD/fuzz/harness_$t.o" $objs -lm \
        -o "/mayhem/$t"
    $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$BUILD/fuzz/standalone_main.o" "$BUILD/fuzz/harness_$t.o" $objs -lm \
        -o "/mayhem/$t-standalone"
done

# 3) Behavioral self-test (test.sh runs it): the same firmware sources with normal flags.
for f in $FIRMWARE; do
    $CC -std=gnu11 -O2 $COVERAGE_FLAGS $WARN $INCLUDES -c "$SRC/src/$f.c" -o "$BUILD/test/$f.o"
done
$CC -std=gnu11 -O2 $COVERAGE_FLAGS $WARN $INCLUDES -c "$SRC/mayhem/shim/shim.c" -o "$BUILD/test/shim.o"
$CC -std=gnu11 -O2 $COVERAGE_FLAGS $WARN $INCLUDES -c "$SRC/mayhem/selftest/selftest.c" \
    -o "$BUILD/test/selftest.o"
$CC $COVERAGE_FLAGS "$BUILD"/test/*.o -lm -o /mayhem/deskhop_selftest
