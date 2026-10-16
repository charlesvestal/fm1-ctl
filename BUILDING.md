# Building Ctl

CTL-1 is an app on [fm1-base](https://github.com/charlesvestal/fm1-base) (`base/`, a git submodule, with
felucca-bsp inside it). Clone with `--recursive`, or after cloning:

```
git submodule update --init --recursive
```

`./build.sh` runs fm1-base's build (`base/tools/build.py`) on `app/`, whose settings are in `app/fm1.toml`. It
makes, in `app/build/`:

| File | What |
| --- | --- |
| `ctl.bin` | the firmware app |
| `loader/ota.bin` | the update loader (felucca-bsp's) |
| `ctl.fwsc` | the installable package (app + loader) |

## Prerequisites (macOS)

- Python 3 with Pillow: `pip3 install Pillow` (Homebrew's Python, with libraqm, for the fonts).
- Docker Desktop, or podman with a `docker` script that runs `exec podman "$@"` first on your PATH. The JieLi
  toolchain is Linux x86-64 only; the build runs it in a `linux/amd64` `debian:bookworm-slim` container
  (Rosetta on Apple silicon). Keep the source tree in a folder the container can share, e.g. under `/Users`.
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server): see fm1-base's README.
- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files (`cpu/wl82/tools/uboot.boot`,
  `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`), and the app links its Bluetooth libraries (`cpu/wl82/liba/btctrler.a`,
  `btstack.a`, `wl_rf_common.a`, two crypto libraries) and compiles three of its config sources. The build copies
  what it needs into `app/build/ble/sdk`; none of it is part of this tree or of git.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Emscripten (optional, for the browser emulator) and Node.js (optional, for its test).

On Linux x86-64 the toolchain runs natively and no container is needed.

## Build

```
./build.sh                    # app/build/ctl.fwsc: USB MIDI, TRS MIDI in and Bluetooth MIDI
./build.sh --no-ble           # app/build/ctl-usb.fwsc: without Bluetooth
./build.sh --release X.Y      # app/build/ctl-X.Y.fwsc, identity FM-1_6XXYYZZ (0.1 is FM-1_6000100)
./build.sh --host             # the simulator: app/build/host/ctl_host (--no-ble: ctl-usb_host)
./build.sh --web              # the browser emulator: app/build/emu/
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations (`~/.jieli/toolchain`, `~/fw-AC79_AIoT_SDK`),
`PYTHON` the Python that runs the build.

Both firmware builds have the same identity, so either installs over the other as an update. Without Bluetooth
(`--no-ble`), fm1-base compiles its `ble_none.c` in place of `ble.c`: none of JieLi's BT libraries and not the
shim are linked (the build fails if any of their symbols ends up in the image), and the UI hides Bluetooth (its
port, state and log).

The build also checks the image, RAM and pool sizes, that register access stays in felucca-bsp, and that no
double-precision code is linked. Two of these are notes with Bluetooth, not failures: JieLi's `.volatile_ram_code`
calls into the flash, and its radio setup may link soft-double routines.

On the FM-1, Bluetooth starts at every boot and advertises as "FM-1" (on a Mac: Audio MIDI Setup, Window, Show
MIDI Studio, the Bluetooth button, Connect). The Setup page shows its state and the stack's log, which survives a
reset. If the last Bluetooth run crashed, it stays off at the next boot ("BLUETOOTH OFF: IT CRASHED"); hold SAVE
on Setup for a second to start it by hand.

## Tests

```
sh tests/run_tests.sh             # CTL-1's
sh base/tests/run_tests.sh        # fm1-base's own (the encoder and TRS parsers, storage, the installer)
```

CTL-1's: the GM synth; the update path against `app/build/ctl.fwsc` (fm1-base's tests); the browser build in
Node (`tests/host/emu_test.mjs`); and the whole app in the simulator (`tests/scenarios/*.ctl`: notes, banks,
scales, the arpeggiator, the sequencer and clock, MIDI thru, Bluetooth, persistence; `tests/scenarios/usb/`
without Bluetooth), with screenshots and the MIDI sent in `app/build/scenarios/`.

`app/build/host/ctl_host SCRIPT OUTDIR` runs one script. The commands are listed at the top of
`base/host/plat_host.c`, and CTL-1's own checks and commands at the top of `app/host.c`. The simulator's
Bluetooth is a stand-in: it is ready a second after the start, and `ble connect` connects a central.

## Install

From the command line (needs `pip3 install mido python-rtmidi`):

```
python3 base/tools/fm1_install.py app/build/ctl.fwsc
python3 base/tools/fm1_install.py --info          # identity of the connected FM-1
```

Installing firmware is at your own risk. Hold OCT− and OCT+ for 5 seconds for the update mode. If the FM-1 no
longer starts but reaches the chip's update mode (4C4A:8057 on USB), `base/bsp/tools/fm1_rescue.sh` puts stock
firmware back from a Mac; otherwise recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## Publishing

```
./build.sh --release X.Y                   # app/build/ctl-X.Y.fwsc, identity FM-1_6XXYYZZ
tools/gen_screens.sh                       # the manual's screenshots, if the screens changed
tools/publish_pages.sh X.Y                 # the site (landing page, installer, manual, emulator) on gh-pages
gh release create vX.Y app/build/ctl-X.Y.fwsc --notes-file NOTES.md
```

The site is https://charlesvestal.github.io/fm1-ctl/ (`tools/make_pages.py`; the manual from `docs/MANUAL.md` by
fm1-base's `tools/manual_html.py`, its panel drawing by `tools/gen_manual_figures.py`).
