# Crash: Ctl with the onboard GM synth (commit 8d73987), at Bluetooth start

Seen on the device 2026-10-08, right after installing 8d73987 ("Onboard GM sounds"). Bluetooth page after
the restart:

```
crash 1 pc 00000070 d 00080000
ret 02008C7C ms 197
e 0000006F 0200767E 02007666 02008C7C
```

Decoded against build/ctl.dis of that build:
- 0x02007666, 0x0200767E: in `gm_render` (dsp/gm.c), around its call into the library
- 0x02008C7C: in `EAS_Render` (dsp/eas/eas_public.c), just after `EAS_MixEnginePost`
- pc 0x70: a jump to near zero: a corrupted return address or function pointer
- ms 197: 197 ms after boot, while Bluetooth starts (ble_start at every boot); after the crash Bluetooth
  stays off (ble_last_bad), and the synth then runs.

What changed in 8d73987: the audio path came back (ALNK0 at priority 3, `render_block` -> `gm_render`, 256
frames a half), Sonivox EAS (~200 KB samples read from XIP, a 96 KB pool in .pool via EAS_HWMalloc), the synth
fed from plat_midi_send. The same synth ran without trouble in fm1-gm (no Bluetooth), ~30% CPU.

Leads, most likely first:
1. **System stack overflow.** The render runs in the ALNK0 interrupt on the 8 KiB system stack
   (_sstack_lo 0x01C7A100 .. 0x01C7C000); the radio's interrupts (priority 5..7) and the 1 kHz tick (4) nest
   on top of it. EAS_Render's frame plus JieLi's radio handlers may not fit. Check: stack-paint the system
   stack and read the high-water mark (fm1-gm alone, then Ctl with Bluetooth); or render the synth outside
   the interrupt (a flag from ALNK0, the main loop renders the next half ahead), or move the render to the
   user stack.
2. **Memory overlap.** JieLi's libraries (or the shim) using RAM that now overlaps EAS's pool (.pool grew
   from 118 KB to 217 KB). Check the BT libraries' sections and any fixed addresses against the map.
3. **Flash access while the radio trims.** The RF trim must run from RAM (app.ld .ram_text): if an
   interrupt running from XIP (the render, which also reads the samples from XIP) lands while the flash is
   unavailable, it would crash. Check whether ble.c masks interrupts below some level during the trim.

To reproduce: install build/ctl.fwsc (8d73987) with Sound on (the default); it crashes at the first boot
with Bluetooth starting. Workaround meanwhile: reinstall d5624a0 (Sound Off does not help: the synth still
renders, silently; skipping the render when Sound is off would be a cheap fix to add anyway).

## Fixed (2026-10-08): lead 3, the synth reading the flash during the RF trim

The last four branches (ETM, newest first: 0x6F, 0x0200767E, 0x02007666, 0x02008C7C) are the returns from
`EAS_MixEnginePost` and `EAS_Render`, the mixing loop's branch in `gm_render`, then a jump to 0x6F: nothing
popped from the stack in between, so not an overflow (lead 1). JieLi's two radio ISRs save r0-r15, psr,
rets and reti; the map has no overlap (lead 2). What was left: the flash misreads while the radio trims
(fm1-ble notes/06). The trim runs from RAM, but the audio interrupt read EAS's code and its 200 KB of
samples from the flash ~30% of the time, right through it.

Fix: `ble_quiet` (ble.c) from `ble_start` until the stack is ready and at least 1 s has passed (or 15 s):
the audio interrupt calls `gm_skip` (silence, the library untouched, MIDI dropped, every note off at the
next render). Sound Off skips the render too. Confirmed on the device: Bluetooth starts, the GM sounds play.
