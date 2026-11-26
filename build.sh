#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
# Build CTL-1 with fm1-base (base/, a git submodule; see BUILDING.md).
#   ./build.sh [--board fm1|smk37] [--model pro|elite|mke] [--release X.Y] [--no-ble] [--host | --web]
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow (and libraqm, for the fonts)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
remaining=$#
while [ "$remaining" -gt 0 ]; do
    case "$1" in
        --board)
            if [ "$remaining" -lt 2 ] || [ -z "$2" ]; then
                echo "build.sh: --board needs a value (fm1 or smk37)" >&2
                exit 2
            fi
            FM1B_BOARD=$2
            export FM1B_BOARD
            shift 2
            remaining=$((remaining - 2))
            ;;
        --board=*)
            FM1B_BOARD=${1#--board=}
            if [ -z "$FM1B_BOARD" ]; then
                echo "build.sh: --board needs a value (fm1 or smk37)" >&2
                exit 2
            fi
            export FM1B_BOARD
            shift
            remaining=$((remaining - 1))
            ;;
        --model)
            if [ "$remaining" -lt 2 ] || [ -z "$2" ]; then
                echo "build.sh: --model needs a value (pro, elite or mke)" >&2
                exit 2
            fi
            FM1B_MODEL=$2
            export FM1B_MODEL
            shift 2
            remaining=$((remaining - 2))
            ;;
        --model=*)
            FM1B_MODEL=${1#--model=}
            if [ -z "$FM1B_MODEL" ]; then
                echo "build.sh: --model needs a value (pro, elite or mke)" >&2
                exit 2
            fi
            export FM1B_MODEL
            shift
            remaining=$((remaining - 1))
            ;;
        *)
            arg=$1
            shift
            set -- "$@" "$arg"
            remaining=$((remaining - 1))
            ;;
    esac
done
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
case " $* " in
    *" --web "*)                            # the emulator: fm1-base's page with CTL-1's bits (tools/emu_page.py)
        "$PY" base/tools/build.py app "$@"
        exec "$PY" tools/emu_page.py;;
esac
exec "$PY" base/tools/build.py app "$@"
