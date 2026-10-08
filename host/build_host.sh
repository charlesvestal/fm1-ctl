#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the Ctl host simulator (the whole app on this machine) into build/host/ctl_host.
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
$CC -O2 -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -DCT_HOST -Ifirmware/src -Ibuild/gen -o build/host/ctl_host host/ctl_host.c
echo "host: build/host/ctl_host"
