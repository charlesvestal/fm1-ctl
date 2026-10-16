# CTL-1 licensing

CTL-1 (Ctl in the code) is free software under the **Mozilla Public License 2.0** (`MPL-2.0`, full text in
`LICENSE`), file by file. If you change one of its files and share the result, share that file's source under the
MPL. It is built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp) (`base/`, a git submodule), both MPL-2.0.

**Both builds may be distributed.** The MPL works file by file, so a firmware made of MPL files may also contain
JieLi's Bluetooth libraries, which are Apache-2.0 and come as binaries only. The build with Bluetooth
(`ctl.fwsc`) links them; the build without it (`./build.sh --no-ble`, `ctl-usb.fwsc`) doesn't, and the build
checks that none of their code is in it. Neither library is part of this repository: the build copies them from
each user's own AC79 SDK.

The browser emulator (`app/web/emu/`, built by fm1-base) runs the host simulator, with a stand-in for the
Bluetooth stack; it contains no JieLi code.

## Where the code comes from

| What | Origin | Licence |
| --- | --- | --- |
| The platform: `base/` (boot, USB, the TRS jack, storage, the update path, screen, panel, key sensing, Bluetooth's shim and MIDI service, the host simulator and the browser emulator, build and install tools) | [fm1-base](https://github.com/charlesvestal/fm1-base) and [felucca-bsp](https://github.com/hugelton/felucca-bsp), from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (@kurogedelic), Hügelton Instruments, under MPL-2.0 with his permission ([felucca-bsp#4](https://github.com/hugelton/felucca-bsp/issues/4)) | MPL-2.0 (the parts of `base/ble/` from JieLi's SDK: Apache-2.0) |
| `web/ctl_installer.html` | Felucca's installer page, by way of X0X and AMB-1 | MPL-2.0 |
| `app/` (`app.c`, `app.h`, `ui.c`, `engine.c`, `project.c`, `sense.c`, `host.c`, `dsp/gm.c`), `app/web/emu/`, `tests/`, `tools/` | Ctl, Charles Vestal | MPL-2.0 |
| The idea of a browser emulator for FM-1 firmware (`app/web/emu/`) | [Groove OS](https://www.groove-os.com/) by Peter Gombos, whose site runs its firmware in the browser; no code from it | credit only |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| The GM synth: `app/dsp/eas/` (Sonivox EAS, its 200 KB GM sound set, `eas_host_fm1.c` our host wrapper). Changed as for Polly: the reverb and chorus sends summed in 32 bits and clamped to 16, and the effects ringing out after the last send and starting clean (`eas_voicemgt.c`, `eas_synth.h`, `eas_reverb.c`, `eas_chorus.c`); each change marked "Ctl (from Polly):" | Apache-2.0, Copyright (c) 2004-2006 Sonic Network Inc., 2008 The Android Open Source Project | `app/dsp/eas/` (`LICENSE-2.0.txt`, `NOTICE`); <https://github.com/EmbeddedSynth/sonivox> |
| Fonts: Barlow Semi Condensed (The Barlow Project Authors), Terminus (Dimitar Toshkov Zhekov) | SIL OFL 1.1 | `base/assets/fonts/` |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; the Bluetooth libraries (`btctrler.a`, `btstack.a`, `wl_rf_common.a`, `crypto_toolbox_Osize.a`, `lib_ccm_aes.a`, binary only), three config sources and the headers are copied into `app/build/` and linked into the app; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. "M-VAVE" and "FM-1" are
trademarks of their respective owners, used here only to say what hardware Ctl runs on. Ctl is
independent firmware; it is not affiliated with, endorsed by or supported by any of them.

## Radio

The build with Bluetooth enables the radio at start (not after a crashed Bluetooth run; then only when SAVE is
held on the Setup page), and only as a BLE peripheral (BLE MIDI). The build without it never enables the radio.
