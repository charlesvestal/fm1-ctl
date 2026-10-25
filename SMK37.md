# CTL-1 on the M-VAVE SMK-37

Control reference for the SMK-37 port of CTL-1.  All FM-1 pages and
functions are available; the extra controls extend rather than replace.

---

## Keys (37-key keyboard)

| Keys | Normal pages | Sequencer page |
|------|-------------|----------------|
| Middle 27 (hw 5–31, F3–G5) | Play notes (FM-1 range) | White: play notes · Black 1–4: select track · Black 5–11: play notes · Black 12–15: mute tracks 1–4 |
| Lower 5 (hw 0–4, C3–E3) | Extend range down (same scale / octave logic) | Same as normal |
| Upper 5 (hw 32–36, G#5–C6) | Extend range up | Same as normal |

> Outer keys use the same chromatic / scale / octave path as the middle 27.

---

## Pads (16 RGB pads, top-row left to right = steps 1–8, bottom = 9–16)

| Always | Sequencer page |
|--------|---------------|
| LEDs mirror the selected track's step pattern in screen colours | Tap a pad to toggle that step on / off |

**Pad colours**

| Colour | Meaning |
|--------|---------|
| Vermilion (K_HOT) | Step active |
| Vermilion tint (K_HOT_T) | Playhead on an active step |
| Mustard (K_COOL) | Playhead on an inactive step |
| Dim warm (barely lit) | Step inactive but within track length |
| Off | Beyond track length |

---

## Encoders

| Knob | Label | Function |
|------|-------|----------|
| K1 | MASTER | Virtual master volume (accumulated; 0–4092 range) |
| K2 | SELECT | Page navigation (Play: bank · Seq: track · Setup: page) |
| K3 | ALGORITHM | MIDI channel (0–15) |
| K4 | PRESETS | Program change (0–127) |
| K5 | KNOB 1 (K5) | Page knob 1 · or fader 1 CC when FADER BANK held |
| K6 | KNOB 2 (K6) | Page knob 2 · or fader 2 CC when FADER BANK held |
| K7 | KNOB 3 (K7) | Page knob 3 · or fader 3 CC when FADER BANK held |
| K8 | KNOB 4 (K8) | Page knob 4 · or fader 4 CC when FADER BANK held |

---

## Faders (4 × motorised-style faders, CC 16–19 by default)

| Behaviour | Detail |
|-----------|--------|
| Default CC | Fader 1 = CC 16, Fader 2 = CC 17, Fader 3 = CC 18, Fader 4 = CC 19 |
| Takeover | Soft: fader only transmits after it passes through the last-sent value |
| Change CC | Hold **FADER BANK** and turn K5–K8 to reassign faders 1–4 to any CC 0–127 |
| Channel | Follows `proj.chan` (same MIDI channel as keys) |

---

## Wheels

| Wheel | MIDI output |
|-------|-------------|
| Pitch wheel | Pitch bend ±2 semitones (±8192 raw); self-calibrating zero (first 32 frames), ±8-count dead-band |
| Mod wheel | CC 1 (mod wheel), 0–127 |

---

## Buttons

All 14 FM-1 buttons are present, remapped to the SMK-37 panel:

| SMK label | FM-1 function | B_* enum |
|-----------|---------------|---------|
| SEQ FX | FX (bank / CC assign) | B_FX |
| GLOBE | SELECT (page nav) | B_SEL |
| PARA | ENV (sustain) | B_ENV |
| FX | LFO (mod wheel up) | B_LFO |
| SEQ | EDIT (Sound page) | B_EDIT |
| CHORD | GLO (Setup page) | B_GLO |
| SCALE | HOME (Play page) | B_HOME |
| PATCH | SAVE | B_SAVE |
| ARP | ARP (Arp page) | B_ARP |
| SEQ REC | SEQ (Sequencer page) | B_SEQ |
| SEQ PLAY | PLAY (start / stop) | B_PLAY |
| NOTE REPEAT | REC (panic) | B_REC |
| OCT− | OCT− | B_OCTDN |
| OCT+ | OCT+ | B_OCTUP |

### Extra SMK-only buttons

| SMK label | Function |
|-----------|----------|
| KNOB BANK | Reserved (B_KNOB_BANK; future bank-switch use) |
| FADER BANK | Hold + K5–K8 to reassign fader CC numbers |
| < | SELECT −1 (previous bank / track / setup page) |
| > | SELECT +1 (next bank / track / setup page) |

---

## Flash map (SMK-37)

| Range | Use | Protected |
|-------|-----|-----------|
| 0x00000–0x03FFF | Boot ROM | ✓ |
| 0x04000–0x9CFFF | App (CTL-1) | |
| 0x9D000–0xBAFFF | VM / store | |
| 0xBB000–0xBFFFF | OTA staging | |
| 0xC0000–0xC0FFF | BTIF | ✓ |
| 0xC1000–0xC1FFF | USRTRIM | ✓ |
| 0xC2000–0xEAFFF | USRFLASH | |
| 0xF4000–0xFDFFF | USR | ✓ |
| 0xFF000–0xFFFFF | key_mac | ✓ |

---

## Building packages

Requires the JieLi AC79 toolchain and SDK (see `BUILDING.md`).

```sh
# SMK-37 Pro (also Starrykey 37 Play)
BOARD=smk37 MODEL=pro   sh build.sh   # → build/ctl1-smk37.fwsc

# SMK-37 Elite
BOARD=smk37 MODEL=elite sh build.sh   # → build/ctl1-smk37-elite.fwsc

# MKE-P37
BOARD=smk37 MODEL=mke   sh build.sh   # → build/ctl1-smk37-mke.fwsc
```

Install with `base/bsp/tools/fm1_install.py` (the fork's web installer also works).
To restore factory firmware: M-Vave's M-UPGRADE tool with the original `.ufw` from
the device backup taken before installing.

---

*Copyright © 2026 Peter Whitney.  Firmware: CTL-1 / smk37-port.yaml.*
