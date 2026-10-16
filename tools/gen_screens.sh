#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# The manual's screenshots: tools/screens.ctl in the host simulator, built as the release is (with Bluetooth),
# into docs/img/screen-*.png.
#   tools/gen_screens.sh
set -e
cd "$(dirname "$0")/.."
sh build.sh --host >/dev/null 2>&1
app/build/host/ctl_host tools/screens.ctl docs/img | grep -v "^  \(ok\|out\)" || true
rm -f docs/img/midi_out.txt                 # (the simulator's MIDI log: not for the manual)
ls docs/img/screen-*.png
