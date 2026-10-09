# Ctl manual

Ctl turns the M-VAVE FM-1 into a MIDI controller. Everything you play goes out over USB and Bluetooth
at the same time, and MIDI coming in is passed along. It also has General MIDI sounds of its own, so it
plays through its speaker and headphone jack with nothing connected.

- [The panel at a glance](#the-panel-at-a-glance)
- [Connecting](#connecting)
- [The screen](#the-screen)
- [Playing](#playing)
- [Knobs and banks](#knobs-and-banks)
- [Scales and chords](#scales-and-chords)
- [Sound](#sound)
- [Arpeggiator](#arpeggiator)
- [Sequencer](#sequencer)
- [Clock and transport](#clock-and-transport)
- [Setup](#setup)
- [Bluetooth](#bluetooth)
- [Saving](#saving)
- [The lights](#the-lights)
- [Safety and updates](#safety-and-updates)
- [MIDI reference](#midi-reference)

## The panel at a glance

| Control | What it does |
| --- | --- |
| Keys | Play notes. Chromatic from F3, or a scale and its chords |
| OCT− / OCT+ | Move the keys down or up an octave, from −3 to +3 |
| KNOB 1–4 | The four values shown at the bottom of the screen |
| SELECT | Tempo, 40 to 240 BPM |
| ALGORITHM | MIDI channel, 1 to 16 |
| PRESETS | Program change, 1 to 128: picks the onboard sound too |
| FX | Next knob bank, A to D. Hold it and turn KNOB 1–4 to set their controller numbers |
| ARP | Arp page |
| ENV | Sustain while held |
| PLAY | Start or stop the sequencer, and send MIDI start or stop |
| REC | Panic: all notes off |
| SAVE | Save the settings now |
| HOME | Play page |
| SEL | Keys page |
| SEQ | Sequencer page |
| GLO | Sound page |
| LFO | Mod wheel: rises while held, falls when let go |
| EDIT | Setup page |
| MASTER | Volume of the onboard sound, or a controller (Sound page) |

Each page has its own button, and that button lights while you're on the page.

## Connecting

- **USB:** plug the FM-1 into a computer. It appears as a class-compliant MIDI device named "Ctl FM-1".
  A Mac that has seen the FM-1 with other firmware may keep showing the old name until the entry is
  removed in Audio MIDI Setup.
- **Bluetooth:** see [Bluetooth](#bluetooth).
- **TRS jack:** this is MIDI in only. The FM-1 has no MIDI out jack.

## The screen

The top line shows the page name and, on the right, the MIDI channel and the tempo. A dot beside the
tempo flashes on each beat; it turns coral while playing. "EXT" replaces the tempo when the clock comes
from outside. Small tags show the knob bank and "BLE" once Bluetooth is running. A small dot after the
page name means there are unsaved changes.

Above the knobs, a status line on every page shows the lowest key's note, "ARP" or "LATCH" while the
arpeggiator is on, "SUSTAIN" while ENV is held, "MOD" while the mod wheel is up, and on the right the program (by name while the onboard
sound is on).

The bottom band shows what KNOB 1–4 do on the current page, with their values. The knob you turned
last is shown in coral for a moment.

Messages, such as "KNOB BANK B" or "SAVED", replace the top line for about a second.

## Playing

The 27 keys play notes from F3 to G5 at octave 0. OCT− and OCT+ move them an octave at a time; the
screen says which note is now the lowest key. A note already held ends correctly even if you change
the octave before letting go.

- **Velocity** is fixed, because the keys don't sense how hard they are pressed. Set it on the
  Keys page (KNOB 3); it starts at 100.
- **Sustain:** hold ENV. It sends controller 64 on, and off when you let go.
- **Mod wheel:** hold LFO. The mod wheel (controller 1) rises from 0 to 127 over a sixteenth note, and when
  you let go it falls back to 0 just as fast. On the onboard sounds it adds vibrato; on a synth it does
  whatever the mod wheel is set to do there. "MOD" shows in the status line while it is up.
- **Channel:** turn ALGORITHM. Any held notes end first, on the channel they started on.
- **Program change:** turn PRESETS. Each step sends a program change at once, and the onboard sound
  follows it: the screen shows the sound's name.
- **Panic:** press REC. It ends every held note and sends sustain off and "all notes off" on the
  keyboard's channel and the sequencer's channel.

The Play page shows the last message you sent, a picture of the keys, and the three ports. Each port
has a teal light for MIDI coming in and, for USB and Bluetooth, a coral light for MIDI going out.

## Knobs and banks

On the Play page, KNOB 1–4 send controllers. There are four banks of four, so 16 controllers in all.
Press FX to step through banks A, B, C and D; the bank letter shows at the top of the screen. Each
knob remembers its value per bank, so a knob picks up where you left it when you switch back.

| Bank | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| A | 74 cutoff | 71 resonance | 1 modulation | 7 volume |
| B | 73 attack | 72 release | 75 decay | 76 vibrato |
| C | 16 | 17 | 18 | 19 |
| D | 20 | 21 | 22 | 23 |

To change a bank's numbers, hold FX and turn KNOB 1–4: while FX is held they set the four controller
numbers of the current bank (0 to 119), and letting go doesn't change the bank. To edit another bank,
tap FX to get to it first.

Turning a knob slowly moves one step per click; turning it quickly moves up to six.

## Scales and chords

Press SEL for the Keys page.

| Knob | Sets |
| --- | --- |
| KNOB 1 Root | C to B |
| KNOB 2 Scale | Chromatic, Major, Minor, Dorian, Mixolydian, Harmonic minor, Pentatonic, Pentatonic minor, Blues |
| KNOB 3 Velocity | 1 to 127 |
| KNOB 4 Humanize | 0 to 40: a random spread of velocity for the arpeggiator and sequencer |

**Chromatic** is the default: the keys work like a normal keyboard.

With any other scale the layout changes:

- **White keys** play the scale's notes in order, starting from the root in octave 3 (C3 for a root
  of C, at octave 0). Every white key sounds right. The keys that play the root glow dimly, so you can
  see where each octave starts.
- **Black keys** play chords: the triad built on each step of the scale. The first black key plays
  the chord on step 1, the second on step 2, and so on. In C major that's C, Dm, Em, F, G, Am, B°, then
  C an octave up.

Changing the root or scale ends any held notes.

## Sound

Ctl has the 128 General MIDI sounds and the drum kit (on channel 10), from Sonivox EAS. They play
whatever Ctl plays: the keys, the arpeggiator and the sequencer. PRESETS picks the sound on the keyboard's
channel.

Press GLO for the Sound page. It shows the sound playing, and how many voices sound and how busy the
processor is.

| Knob | Sets |
| --- | --- |
| KNOB 1 Sound | On or Off |
| KNOB 2 MIDI in | Plays or Ignored: whether MIDI coming in plays the sounds too |
| KNOB 3 MASTER | Volume, or a controller number for the MASTER knob to send |

**MASTER** is a normal knob with a fixed range, unlike KNOB 1–4, which turn endlessly. Set to Volume
(the default), it is the volume of the onboard sounds. Given a controller number, it sends from 0 to 127
across its travel, only when it moves, and the onboard sounds play at full volume.

Up to 24 notes sound at once.

## Arpeggiator

Press ARP for the Arp page; KNOB 1 there turns the arpeggiator on. "ARP" (or "LATCH") shows in the
status line while it's on. While it's on, the keys don't sound
directly. Hold notes or a chord and the arpeggiator plays them one at a time in tempo. With a scale
chosen, one black key is a whole chord to arpeggiate.

| Knob | Sets |
| --- | --- |
| KNOB 1 Arp | Off, On or Latch |
| KNOB 2 Mode | Up, Down, Up-down, Random, As played |
| KNOB 3 Rate | 1/4, 1/8, 1/8 triplet, 1/16, 1/16 triplet, 1/32 |
| KNOB 4 Octaves | 1 to 3: the notes repeated in higher octaves |

- **On:** the arpeggiator plays while you hold keys and stops when you let go.
- **Latch:** it keeps playing after you let go. The next chord you play, after letting go of
  everything, replaces the old one.


Each note lasts half a step. The arpeggiator follows the clock, and runs whether or not the
sequencer is playing. Its steps fall on the same grid as the sequencer's, so the two stay in time: a
chord played between steps starts on the next one. The Arp page shows the notes it is playing, with the current one bright.

## Sequencer

Press SEQ for the Sequencer page. It has four drum tracks that play sixteenth notes on the
sequencer's channel, 10 by default. Each row on screen is one track.

| Track | Note | Steps | Hits |
| --- | --- | --- | --- |
| 1 | 36 Kick | 16 | 4 |
| 2 | 38 Snare | 16 | 2, turned 4 steps |
| 3 | 42 Hat | 16 | 8 |
| 4 | 39 Clap | 12 | 3, turned 2 steps |

KNOB 1–4 set the selected track:

| Knob | Sets |
| --- | --- |
| KNOB 1 Steps | 1 to 16: the track's length |
| KNOB 2 Hits | How many steps play |
| KNOB 3 Rotate | Turns the pattern, step by step |
| KNOB 4 Note | The note it plays; on channel 10 the screen shows the drum name |

The hits are spread as evenly as possible over the steps, which is known as a Euclidean rhythm. Tracks
of different lengths drift against each other, which makes longer, shifting patterns.

**Editing by hand:** on this page the keys edit instead of playing.

- **White keys** turn the selected track's 16 steps on or off.
- **Black keys 1–4** select a track.
- **Black keys 5–8** mute or unmute tracks 1–4.

Turning Steps, Hits or Rotate rebuilds the pattern from those numbers, which replaces any steps you
set by hand. Set the numbers first, then fine-tune with the keys.

The lights on this page show the pattern: steps that play are bright, empty steps are dim, and the
step playing now is flipped. The selected track's black key is bright, and the mute keys are lit for
tracks that are playing.

Press PLAY to start and stop. The sequencer always starts from step 1. Velocity comes from the Keys
page, spread by Humanize.

## Clock and transport

Ctl keeps a clock at 24 ticks per beat. The arpeggiator and sequencer run on it. Choose its source on
the Setup page (KNOB 1):

| Clock | What happens |
| --- | --- |
| Int | Ctl keeps time at the SELECT tempo and sends MIDI clock: over USB all the time, over Bluetooth while playing. The default |
| Int, quiet | Ctl keeps time but sends no clock |
| Ext | Ctl follows MIDI clock and start / stop from any input: USB, Bluetooth or the TRS jack |

PLAY starts and stops the sequencer. With an internal clock it also sends MIDI start and stop, so a
DAW or drum machine following the clock starts with it. With Ext, incoming start and stop drive the
sequencer, SELECT does nothing, and the screen shows "EXT".

## Setup

Press EDIT for the Setup page.

| Knob | Sets |
| --- | --- |
| KNOB 1 Clock | Int, Int quiet, Ext (see above) |
| KNOB 2 Seq ch | The sequencer's MIDI channel, 1 to 16 |
| KNOB 3 Glow | Off, Low or High: how brightly unlit buttons glow |
| KNOB 4 Bluetooth | Shows whether it is on; starts it after a crash (see below) |

The page also shows Bluetooth's state and log, and the firmware version.

The USB-only version of Ctl (`ctl-usb`) has no Bluetooth: KNOB 4 is empty here, the page shows only the
firmware version, and the Play page shows only the USB and TRS ports.

## Bluetooth

Bluetooth starts by itself every time the FM-1 starts, as on the stock firmware. It advertises as
"FM-1"; the Setup page (press EDIT) shows "advertising", or "connected" with the messages counted in
and out.

On a Mac: open Audio MIDI Setup, choose Window > Show MIDI Studio, click the Bluetooth button, find
"FM-1" and click Connect. The FM-1 then appears as a MIDI device in any app.

Every message carries the moment it happened, so apps that use MIDI timestamps (most DAWs, when they
record) get the timing as exact as over USB. Apps that play notes as they arrive hear Bluetooth's own
unevenness, a few milliseconds to a few tens. Wi-Fi on 5 GHz and the FM-1 near the computer help.
Nothing is queued while no device is connected, so notes played meanwhile are not sent later.

The Setup page also shows the Bluetooth log, which survives a restart. If Bluetooth ever crashes the
FM-1, the next start leaves Bluetooth off, says so, and the log shows a crash note; KNOB 4 on the Setup
page then starts Bluetooth by hand.

## Saving

Ctl saves your settings by itself about four seconds after a change, once nothing is held and the
sequencer is stopped. Press SAVE to save at once. Everything on the pages is saved: knob banks and
values, scale, sound, arpeggiator, sequencer tracks, channel, tempo, octave and Setup.

Saving pauses the FM-1 for a moment, which is why it waits until you stop playing.

## The lights

- **Bright:** the current page's button, LFO while the mod wheel is up, ENV while sustain is held,
  and OCT− or OCT+ while the octave is shifted.
- **PLAY** is green: it flashes bright green on each beat while playing, and glows dim green otherwise.
- **Keys:** light while you hold them, and when a note arrives from MIDI in.
- **Dim (Glow):** every other function button, so the labels can be read in the dark. With a scale,
  the root keys glow too.

## Safety and updates

These come from Felucca, the platform Ctl is built on, and work as in the other FM-1 firmwares:

- **Update mode:** hold OCT− and OCT+ for 5 seconds. The FM-1 restarts ready for an update.
- **Safe mode:** if the FM-1 fails to start twice in a row, it starts in safe mode with only USB on,
  so the web installer can reach it. PLAY tries a normal start again.
- **Button calibration:** hold OCT− and OCT+ while switching on, then press each button and turn each
  knob as the screen asks. Only needed if buttons do the wrong thing.

## MIDI reference

What Ctl sends, all on the keyboard channel unless noted:

| Action | Message |
| --- | --- |
| Key | Note on (velocity from the Keys page), note off |
| KNOB 1–4 on Play | Control change, the bank's controller |
| MASTER | Control change, the Sound page's controller (when it has one) |
| PRESETS | Program change |
| ENV | Control change 64, 127 then 0 |
| LFO | Control change 1, rising to 127 over a sixteenth, then back to 0 |
| REC | Note offs, control change 64 = 0, control change 123 = 0 (also on the sequencer channel) |
| Arpeggiator | Note on and off |
| Sequencer | Note on and off on the sequencer channel |
| Clock (Int) | MIDI clock, 24 per beat: over USB all the time, over Bluetooth while playing |
| PLAY (Int, Int quiet) | MIDI start / stop |

How MIDI coming in is passed along:

| From | To |
| --- | --- |
| USB | Bluetooth |
| Bluetooth | USB |
| TRS jack | USB and Bluetooth |

Ctl passes on channel messages and real-time messages (clock, start, stop). It does not pass on
SysEx.
