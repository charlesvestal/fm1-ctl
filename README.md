# Ctl: a MIDI controller for the M-VAVE FM-1

PRIVATE. Never push this repository anywhere public, and do not distribute its firmware: it links
JieLi's Bluetooth libraries (Apache-2.0, binary only) into a GPL-3.0 (Felucca-derived) firmware,
which cannot then offer its complete corresponding source. Distribution waits on a GPL section 7
additional permission from Felucca's copyright holders (asked), or another route (see LICENSING.md).

Ctl turns the FM-1 into a MIDI controller: the keys play notes (in a scale, if you like), the knobs
send controllers in four banks, an arpeggiator and a four-track step sequencer run on its clock, and
everything goes out over USB and Bluetooth (BLE MIDI) at once. MIDI coming in is passed on: USB to
Bluetooth, Bluetooth to USB, and the TRS jack (MIDI in) to both. No sound.

| Control | What it does |
| --- | --- |
| Keys | Notes, chromatic from F3; with a scale, the white keys play its steps and the black keys its chords |
| OCT− / OCT+ | The keys an octave down / up (−3 to +3) |
| KNOB 1–4 | The page's four values; on Play, the knob bank's four controllers |
| FX | The next knob bank (A–D; their controller numbers on the Knobs page) |
| SELECT / ALGORITHM / PRESETS | Tempo, MIDI channel, program change |
| ARP | The arpeggiator on / off (hold notes or chords) |
| ENV | Sustain while held |
| PLAY | Start / stop: the sequencer, and MIDI start / stop |
| REC | All notes off |
| HOME / SEL / LFO / SEQ | The Play, Scale, Arp and Sequencer pages |
| GLO | Knobs, then Setup, then Bluetooth; EDIT steps through every page |
| SAVE | Saves; Ctl also saves by itself a few seconds after a change (not while playing) |

- **Scale page:** root, scale (nine, chromatic first), velocity, humanize (a velocity spread for the
  arpeggiator and sequencer).
- **Arp page:** mode (up, down, up-down, random, as played), rate (1/4 to 1/32), octaves, latch.
- **Sequencer page:** four tracks on the sequencer channel (10: drums). KNOB 1–4 set the selected track's
  steps, hits, rotation and note; the hits are spread evenly (a Euclidean rhythm), and each track can have
  its own length. The keys edit here: white keys toggle steps, black keys 1–4 pick a track, 5–8 mute one.
- **Setup page:** the clock (internal and sent out, internal, or external from any MIDI input), the
  sequencer's channel, MASTER as a controller, and Glow: unlit buttons glow dimly so they can be read.

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
