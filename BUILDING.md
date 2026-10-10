# Building Ctl

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `ctl.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `ctl.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow`
- Docker Desktop. The JieLi toolchain is Linux x86-64 only; the build runs each tool in a
  `linux/amd64` `debian:bookworm-slim` container (Rosetta on Apple silicon). Keep the source
  tree in a folder Docker can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`), and the app links its
  Bluetooth libraries (`cpu/wl82/liba/btctrler.a`, `btstack.a`, `wl_rf_common.a`, two crypto
  libraries) and compiles three of its config sources. The build copies what it needs into
  `build/ble/sdk`; none of it is part of this tree or of git.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web tests).

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`).

`./build.sh --release 0.1` makes a release build; the package is `build/ctl-0.1.fwsc`, identity FM-1_6000100.

`./build.sh --no-ble` makes the USB-only build (`build/ctl-usb.fwsc`, `ctl-usb-0.1.fwsc` with
`--release 0.1`): `CT_BLE=0`, `firmware/src/app/ble_none.c` in place of `ble.c`, none of JieLi's BT
libraries or the shim linked (the build fails if any of their symbols is in the image), and the
Bluetooth knob, port and log hidden. It keeps the same identity, so a Bluetooth build installs over it as
an update. `CT_BLE=0 sh host/build_host.sh` and `CT_BLE=0 web/emu/build.sh` build the simulator and the
emulator to match; `tests/scenarios/usb/` runs against it.

On macOS with podman instead of Docker, put a `docker` script that runs `exec podman "$@"` first
on your PATH. `CT_JOBS` (default 4) limits parallel compiles: a podman machine drops
connections when many containers start at once.

Build option: `CT_CDC=1` adds Felucca's USB serial function (off by default: one plain
MIDI interface).

The build also generates `build/gen/` (the font) and checks the image, RAM and pool sizes and that
register access stays in `hal/`. Two of Felucca's checks are notes here, not failures: JieLi's
`.volatile_ram_code` calls into XIP, and its radio setup may link soft-double routines.

## Tests

```
tests/run_tests.sh
```

Runs, on the build machine: the encoder and TRS parsers; flash storage (and the object map); Felucca's
update-path tests against `build/ctl.fwsc`; the browser build in Node (`tests/host/emu_test.mjs`);
and the whole app in the simulator (`tests/scenarios/*.ctl`: notes, banks, scales, the arpeggiator,
the sequencer and clock, MIDI thru, Bluetooth, persistence), with screenshots and the MIDI sent in `build/scenarios/`.

`host/build_host.sh` builds the simulator alone; `build/host/ctl_host SCRIPT OUTDIR` runs one
script (the command list is at the top of `host/ctl_host.c`). The simulator's Bluetooth is a
stand-in: it is ready a second after the start, and `ble connect` connects a central.

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 tools/fm1_install.py build/ctl.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for Felucca's update
mode. If the FM-1 no longer starts but reaches the chip's update mode (4C4A:8057 on USB),
`tools/fm1_rescue.sh` puts stock firmware back from a Mac; otherwise recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Publishing

Only the USB-only build is released (the Bluetooth build may not be distributed while its licence is
open: README.md, LICENSING.md):

```
./build.sh --no-ble --release X.Y          # build/ctl-usb-X.Y.fwsc, identity FM-1_6XXYYZZ
tools/gen_screens.sh                       # the manual's screenshots, if the screens changed
tools/publish_pages.sh X.Y                 # the site (landing page, installer, manual, emulator) on gh-pages
gh release create vX.Y build/ctl-usb-X.Y.fwsc --notes-file NOTES.md
```

The site is https://charlesvestal.github.io/fm1-ctl/ (`tools/make_pages.py`; the manual from
`docs/MANUAL.md` by `tools/manual_html.py`, its panel drawing by `tools/gen_manual_figures.py`).
