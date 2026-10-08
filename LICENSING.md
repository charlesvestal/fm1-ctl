# Ctl licensing

Ctl's own code is free software under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). It is built on Felucca's platform layer (through X0X and
FoMni) and keeps Felucca's licence.

**The source can be shared; the built firmware cannot (yet).** The firmware links JieLi's
Bluetooth libraries, which are Apache-2.0 but binary only. A GPLv3 binary that contains them cannot
come with its complete corresponding source, so nobody may distribute a built Ctl (`ctl.bin`,
`ctl.fwsc`, or any build of it), you or anyone who builds it. The source in this repository may be
shared under its licences: JieLi's libraries are not part of it, and the build takes them from each
user's own copy of the AC79 SDK. Building it for your own use is fine. The routes to distributing the
firmware: a GPLv3 section 7 additional permission from Felucca's copyright holders allowing the link
(asked), an MIT rewrite of the platform layer, or a Bluetooth part running as a separate program.

The browser emulator (`web/emu/`) is built from the host simulator, which leaves the Bluetooth stack
out; it contains no JieLi code and may be shared as it is.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| Platform: `firmware/hal/`, `firmware/loader/`, `firmware/src/{libc,lcd,gfx,usb,storage,ota,midi_uart}.c`, `tools/` (build, package, install, rescue), `web/fm1*.js` | [Felucca](https://github.com/hugelton/Felucca), Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, as changed for [X0X](https://github.com/charlesvestal/fm1-x0x) and FoMni | GPL-3.0-only |
| `firmware/src/app/{panel,plat_fm1,main_fm1}.c`, `host/ctl_host.c`, `web/emu/` | adapted from FoMni and X0X (themselves from Felucca's `panel.c`, `main.c`, `audio.c`) | GPL-3.0-only |
| `ble/shim/`, `ble/midi/le_midi.c`, `firmware/src/app/ble.c` | fm1-ble (the Bluetooth experiment this firmware grew from) | GPL-3.0-only, with the JieLi parts below |
| Parts of `ble/midi/le_midi.c` (from the SDK's `apps/common/ble/le_trans_data.c`, changed), `ble/shim/rf_const.c` (values from `apps/common/net/wifi_conf.c`), and the interface details repeated in `ble/shim/shim.h` and `ble/shim/misc.c` (each file's header says which) | [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK), Copyright (c) JIELI (Zhuhai Jieli Technology Co., Ltd.) | Apache-2.0 (`LICENSES/Apache-2.0.txt`) |
| The idea of a browser emulator for FM-1 firmware (`web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |
| Everything else in `firmware/src/app`, `tests/` | Ctl | GPL-3.0-only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Barlow Semi Condensed (The Barlow Project Authors), the UI face | SIL OFL 1.1 | `assets/fonts/BarlowSemiCondensed-*.ttf`, `assets/fonts/Barlow-OFL.txt` |
| Terminus (Dimitar Toshkov Zhekov), an alternative font set | SIL OFL 1.1 | `assets/fonts/ter-u*.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; the Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, `crypto_toolbox_Osize.a`, `lib_ccm_aes.a`, binary only), three config sources and the headers are copied into `build/` and linked into the app; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are
trademarks of their respective owners, used here only to say what hardware Ctl runs on. Ctl is
independent firmware; it is not affiliated with, endorsed by or supported by any of them.

## Radio

Ctl enables the Bluetooth radio only when asked (the Bluetooth page), never at start, and only as a
BLE peripheral (BLE MIDI).
