#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Ctl in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [CT_VERSION=0.1] [CT_BLE=0] web/emu/build.sh   (CT_BLE=0: as the USB-only firmware)  ->  build/emu/{index.html, worklet.js, ctl.wasm, words.js, tts.js, tts_worker.js, espeak/}
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
emcc -O2 -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DCT_HOST -DCT_WEB -DCT_BLE=${CT_BLE:-1} "-DCT_VERSION=\"$(printf %s "${CT_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ifirmware/src/dsp/eas -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/ctl.wasm web/emu/ctl_web.c firmware/src/dsp/gm.c $(ls firmware/src/dsp/eas/*.c | grep -v "wt_22khz\|wt_200k_samples")
cp web/emu/index.html web/emu/worklet.js build/emu/
# the screen reader's words and voice: fm1-reader's web/ (words.js, tts.js and eSpeak NG, GPL-3.0), one copy,
# brought in here (FM1_READER: its checkout, default ../fm1-reader). Without it the page reads the screen's
# text as it is, and has no voice of its own
R="${FM1_READER:-../fm1-reader}/web"
if [ -f "$R/words.js" ] && [ -f "$R/espeak/espeak.wasm" ]; then
    mkdir -p build/emu/espeak
    cp "$R/words.js" "$R/tts.js" "$R/tts_worker.js" build/emu/
    cp "$R/espeak/espeak.js" "$R/espeak/espeak.wasm" "$R/espeak/LICENSE" build/emu/espeak/
else
    rm -rf build/emu/words.js build/emu/tts.js build/emu/tts_worker.js build/emu/espeak
    echo "emu: no fm1-reader in $R: the screen is read as its text, without the page's voice"
fi
echo "emu: build/emu ($(wc -c < build/emu/ctl.wasm) B wasm)"
