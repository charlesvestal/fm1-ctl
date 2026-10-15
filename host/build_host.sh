#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the Ctl host simulator (the whole app on this machine) into build/host/ctl_host;
# CT_BLE=0: the USB-only build (no Bluetooth), into build/host/ctl_host_usb.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
BLE="${CT_BLE:-1}"
EXE=build/host/ctl_host
[ "$BLE" = 1 ] || EXE=build/host/ctl_host_usb
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
$CC -O2 -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -DCT_HOST -DCT_BLE=$BLE -Ifirmware/src -Ifirmware/src/dsp -Ifirmware/src/dsp/eas -Ibuild/gen -o $EXE host/ctl_host.c \
    firmware/src/dsp/gm.c $(ls firmware/src/dsp/eas/*.c | grep -v "wt_22khz\|wt_200k_samples") -lm
echo "host: $EXE"
