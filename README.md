# Ctl: a MIDI controller for the M-VAVE FM-1

**Share the source, not the firmware.** Ctl links JieLi's Bluetooth libraries (Apache-2.0, binary only)
into a GPL-3.0 (Felucca-derived) firmware, which then cannot come with its complete corresponding
source. So no built Ctl may be distributed, by anyone, until that is settled (a GPL section 7
permission from Felucca's copyright holders has been asked for). The source here may be shared under
its licences, and anyone may build it for themselves. See [LICENSING.md](LICENSING.md).

Ctl turns the FM-1 into a MIDI controller: the keys play notes (in a scale, if you like), the knobs
send controllers in four banks, an arpeggiator and a four-track step sequencer run on its clock, and
everything goes out over USB and Bluetooth (BLE MIDI) at once. MIDI coming in is passed on: USB to
Bluetooth, Bluetooth to USB, and the TRS jack (MIDI in) to both. No sound.

**[MANUAL.md](MANUAL.md)** explains every control, page and setting.

Bluetooth starts at every boot, as on the stock firmware, and advertises as "FM-1". On a Mac: Audio
MIDI Setup, Window, Show MIDI Studio, the Bluetooth button, then Connect. The Bluetooth page shows the
stack's log, which survives a reset (after a crash it starts with the crash note).

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
