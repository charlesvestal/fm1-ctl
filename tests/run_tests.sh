#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# CTL-1's tests: the GM synth, the whole app in fm1-base's simulator (tests/scenarios/*.ctl: notes, controllers,
# MIDI thru, Bluetooth, persistence; tests/scenarios/usb/: the build without Bluetooth), the browser build, and the
# update path against CTL-1's package (./build.sh first). Screenshots and the MIDI sent in app/build/scenarios/.
# fm1-base's own tests: base/tests/run_tests.sh.
#   tests/run_tests.sh
set -u
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
PY="${PYTHON:-python3}"
B=app/build
OUT=$B/tests
FAIL=0
mkdir -p "$OUT" $B/scenarios
run() {
    name="$1"; shift
    if "$@" > "$OUT/$name.log" 2>&1; then
        echo "  ok   $name"
    else
        echo "  FAIL $name (see $OUT/$name.log)"
        tail -15 "$OUT/$name.log" | sed 's/^/       /'
        FAIL=1
    fi
}
run gm sh -c "$CC -O2 -std=gnu99 -w -DPLAT_HOST -Iapp -Iapp/dsp -Iapp/dsp/eas -Ibase/include -o $OUT/gm_test tests/host/gm_test.c \
    \$(ls app/dsp/eas/*.c | grep -v 'wt_22khz\\|wt_200k_samples') -lm && $OUT/gm_test"
if [ -f $B/ctl.fwsc ]; then
    run ota-entry sh -c "$CC -w -o $OUT/ota_test base/tests/ota_test.c && $OUT/ota_test $B/ctl.fwsc"
    run update-loader sh -c "head -c 100000 $B/ctl.bin > $OUT/old_app.bin && \
        $PY base/tools/fm1pkg_make.py $OUT/old_app.bin $B/loader/ota.bin $OUT/old.fwsc >/dev/null && \
        $CC -w -o $OUT/ldr_test base/tests/ldr_test.c && $OUT/ldr_test $OUT/old.fwsc $B/ctl.fwsc"
else
    echo "  skip update path (no $B/ctl.fwsc: ./build.sh)"
fi
run host-build sh build.sh --host
run host-build-usb sh build.sh --host --no-ble
if command -v emcc >/dev/null 2>&1; then
    run emu sh -c "sh build.sh --web && node tests/host/emu_test.mjs $B/emu/app.wasm"
else
    echo "  skip emu (no emcc)"
fi
for s in tests/scenarios/*.ctl; do
    n=$(basename "$s" .ctl)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/ctl_host "$s" "$B/scenarios/$n"
done
for s in tests/scenarios/usb/*.ctl; do       # the build without Bluetooth: its controls hidden
    n=usb-$(basename "$s" .ctl)
    mkdir -p "$B/scenarios/$n"
    run "scenario-$n" $B/host/ctl-usb_host "$s" "$B/scenarios/$n"
done
[ $FAIL -eq 0 ] && echo "all tests passed" || echo "TESTS FAILED"
exit $FAIL
