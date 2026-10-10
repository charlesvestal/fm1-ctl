#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The manual's screenshots: tools/screens.ctl in the host simulator, built as the release is (USB only,
# CT_BLE=0), into docs/img/screen-*.png.
#   tools/gen_screens.sh
set -e
cd "$(dirname "$0")/.."
CT_BLE=0 sh host/build_host.sh >/dev/null 2>&1
build/host/ctl_host_usb tools/screens.ctl docs/img | grep -v "^  \(ok\|out\)" || true
rm -f docs/img/midi_out.txt                 # (the simulator's MIDI log: not for the manual)
ls docs/img/screen-*.png
