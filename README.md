# Ctl: a MIDI controller for the M-VAVE FM-1

PRIVATE. Never push this repository anywhere public, and do not distribute its firmware: it links
JieLi's Bluetooth libraries (Apache-2.0, binary only) into a GPL-3.0 (Felucca-derived) firmware,
which cannot then offer its complete corresponding source. Distribution waits on a GPL section 7
additional permission from Felucca's copyright holders (asked), or another route (see LICENSING.md).

Ctl turns the FM-1 into a plain MIDI controller: the keys play notes, the knobs send controllers,
and everything goes out over USB and Bluetooth (BLE MIDI) at once. MIDI coming in is passed on:
USB to Bluetooth, Bluetooth to USB, and the TRS jack (MIDI in) to both. No sound.

| Control | What it does |
| --- | --- |
| Keys | Notes, chromatic, F3 to G5 at octave 0 |
| OCT− / OCT+ | The keys an octave down / up (−3 to +3) |
| KNOB 1–4 | Four controllers: 74, 71, 1 and 7 at first (the numbers are on the Setup page) |
| SELECT | Velocity |
| ALGORITHM | MIDI channel |
| PRESETS | Program change |
| ARP | Sustain while held |
| ENV / LFO | Pitch bend down / up while held |
| PLAY | MIDI start / stop |
| REC | All notes off |
| HOME / GLO / SEL | The Play, Setup and Bluetooth pages (EDIT steps through them) |
| SAVE | Saves; Ctl also saves by itself a few seconds after a change |

Bluetooth is off at every start. Turn KNOB 1 on the Bluetooth page to start it; it advertises as
"FM-1" and stays on until the next restart. On a Mac: Audio MIDI Setup, Window, Show MIDI Studio,
the Bluetooth button, then Connect. The Bluetooth page also shows the stack's log, which survives a
reset (after a crash it starts with the crash note).

Hold OCT− and OCT+ for 5 seconds for Felucca's update mode. Two failed starts in a row give safe
mode (USB on, nothing else), from where the web installer works.

## Build and test

```
./build.sh                                     # build/ctl.fwsc (needs the AC79 SDK: the BT libraries)
sh tests/run_tests.sh                          # host tests, the app in the simulator, the web build
python3 tools/fm1_install.py build/ctl.fwsc    # install it over USB
```

`build/host/ctl_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.ctl`), with screenshots and the MIDI sent coming out. `web/emu/build.sh` builds
the browser emulator, which can play a real MIDI port through Web MIDI.

## Where it comes from

Ctl is FoMni's platform layer (Felucca's, through X0X) with a new app, plus the BLE MIDI pieces from
fm1-ble: the shim that runs JieLi's BT stack without JieLi's OS (`ble/shim`), the BLE MIDI profile
(`ble/midi`) and the glue (`firmware/src/app/ble.c`). fm1-ble's notes explain the start-up
workaround and the crashes it avoids.
