#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Ctl in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [CT_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, ctl.wasm}
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
emcc -O2 -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DCT_HOST -DCT_WEB "-DCT_VERSION=\"$(printf %s "${CT_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/ctl.wasm web/emu/ctl_web.c
cp web/emu/index.html web/emu/worklet.js build/emu/
echo "emu: build/emu ($(wc -c < build/emu/ctl.wasm) B wasm)"
