# CTL-1 for the M-VAVE FM-1

CTL-1 turns the M-VAVE FM-1 into a MIDI controller. The keys play notes, in any scale with its chords
under the black keys; the knobs send controllers in four banks; an arpeggiator and a four-track
Euclidean drum sequencer run on its clock. Everything goes out over USB, MIDI from the TRS jack is passed
along, and the FM-1 plays it all on its own General MIDI sounds too, so it works with nothing connected.

![The Play page](docs/img/screen-play.png) ![The Keys page](docs/img/screen-keys.png) ![The Sequencer page](docs/img/screen-seq.png)

- **Try it in your browser** (no FM-1 needed): <https://charlesvestal.github.io/fm1-ctl/emu/>
- **Install it** (Chrome or Edge, with the FM-1 plugged in): <https://charlesvestal.github.io/fm1-ctl/install/>
- **Read the manual**: <https://charlesvestal.github.io/fm1-ctl/manual/> (source: [docs/MANUAL.md](docs/MANUAL.md))
- **Download the firmware file**: [releases](https://github.com/charlesvestal/fm1-ctl/releases)

Installing it is at your own risk, though it's hard to get stuck: the web installer can put
M-VAVE's own firmware back, and an FM-1 that fails to start twice comes up in a safe mode the
installer can reach.

## In short

| | |
|---|---|
| Keys | Notes; with a scale, its notes on the white keys and its chords on the black keys |
| KNOB 1–4 | The page's four values; on Play, four controllers in four banks |
| HOME · SEL · ARP · SEQ · GLO · EDIT | The pages: Play, Keys, Arp, Sequencer, Sound, Setup |
| SELECT | The page's own pages: the banks on Play, the tracks on the Sequencer |
| ALGORITHM · PRESETS · MASTER | MIDI channel · program change (the sound follows) · volume |
| FX | The next bank; held, KNOB 1–4 set its controller numbers |
| ENV · LFO | Sustain (tap twice to lock) · mod wheel |
| PLAY · REC · SAVE | Start / stop · panic · save |
| OCT− / OCT+ | An octave down or up |

The [manual](https://charlesvestal.github.io/fm1-ctl/manual/) has the rest.

## Two builds

The released firmware is the **USB-only build**: USB MIDI and the TRS jack's MIDI in. This repository
also holds a **Bluetooth build** (BLE MIDI both ways, on JieLi's Bluetooth libraries), which may not be
distributed yet: those libraries are binary-only, which a GPL-3.0 firmware can't carry until a GPL
section 7 permission from Felucca's copyright holders allows it (asked). The source of both may be
shared, and anyone may build the Bluetooth one for themselves. See [LICENSING.md](LICENSING.md).

## Building and testing

The toolchain is the same as X0X's (see [BUILDING.md](BUILDING.md)):

```
sh tests/run_tests.sh                          # host tests, the app in the simulator, the web build
./build.sh --no-ble                            # build/ctl-usb.fwsc: the USB-only build
./build.sh                                     # build/ctl.fwsc: with Bluetooth (needs the AC79 SDK's BT libraries)
python3 tools/fm1_install.py build/ctl-usb.fwsc    # install it over USB
```

`build/host/ctl_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.ctl`), with screenshots and the MIDI sent coming out; `tools/gen_screens.sh` makes the
manual's screenshots. `CT_BLE=0 web/emu/build.sh` builds the same code for the browser (the emulator),
which can play a real MIDI port through Web MIDI.

On the Bluetooth build: it starts at every boot and advertises as "FM-1" (on a Mac: Audio MIDI Setup,
Window, Show MIDI Studio, the Bluetooth button, Connect); the Setup page shows the stack's log, which
survives a reset. Its pieces come from fm1-ble: the shim that runs JieLi's BT stack without JieLi's OS
(`ble/shim`), the BLE MIDI profile (`ble/midi`) and the glue (`firmware/src/app/ble.c`).

## Credits

The platform (hardware layer, USB, update loader, storage, installer) is
[Felucca](https://github.com/hugelton/Felucca)'s by Leo Kuroshita (Hügelton Instruments), by way of
[X0X](https://github.com/charlesvestal/fm1-x0x) and [FoMni-1](https://github.com/charlesvestal/fm1-fomni);
the top bar and knob row are X0X's. The General MIDI sounds are Sonivox EAS (Apache-2.0). The Euclidean
rhythms are Godfried Toussaint's reading of Bjorklund's algorithm. The idea of running FM-1 firmware in
the browser comes from [Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).
