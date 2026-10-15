/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl app: the settings (everything that is saved), the engine (clock, arpeggiator, sequencer) and the
 * app's entry points. Included by the unity build after gfx.c (drawing helpers in scope) and by the
 * host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/gm.h"

#ifndef CT_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define CT_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x4C544331u           /* "CTL1" */
#define PROJ_FORMAT 2u                   /* 2: banks, scale, arpeggiator, sequencer (a format 1 gives the defaults) */
#define NKNOBS 4
#define NBANKS 4
#define NTRACKS 4
#define NSCALES 9

enum { CLK_INT_OUT, CLK_INT, CLK_EXT, NCLK };
enum { SOUND_IN, SOUND_KEYS, SOUND_OFF, NSOUND };   /* the onboard GM synth plays: Ctl's notes and MIDI in / Ctl's / nothing */            /* internal clock sent out / kept in / followed from MIDI in */
enum { AT_OFF, AT_POLY, AT_CHAN, NAT };   /* aftertouch from the keys: none / per note / the channel's */
enum { ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_PLAYED, NARP };
enum { RATE_4, RATE_8, RATE_8T, RATE_16, RATE_16T, RATE_32, NRATE };

typedef struct {
    uint8_t steps, hits, rot, note;      /* the pattern is regenerated from these when one changes */
    uint16_t pat;                        /* step bits, edited by hand on the Sequencer page */
    uint8_t mute, rsv;
} track_t;

typedef struct {
    uint32_t magic, format;
    uint8_t chan;                        /* MIDI channel 0..15 (shown 1..16) */
    uint8_t vel;                         /* note-on velocity 1..127 */
    int8_t octave;                       /* -3..+3 */
    uint8_t program;                     /* the last program change sent, 0..127 */
    uint8_t bank;                        /* the knob bank playing, 0..3 (A..D) */
    uint8_t root, scale;                 /* scale 0 = chromatic (the keys as a keyboard) */
    uint8_t human;                       /* velocity spread 0..40 for the arpeggiator and sequencer */
    uint8_t cc[NBANKS][NKNOBS];          /* KNOB 1-4 per bank: controller numbers 0..119 */
    uint8_t val[NBANKS][NKNOBS];         /* and their values 0..127 */
    uint8_t tempo;                       /* 40..240 BPM */
    uint8_t clock;                       /* CLK_* */
    uint8_t arp_on, arp_mode, arp_rate, arp_oct, arp_latch;
    uint8_t seq_ch;                      /* the sequencer's channel 0..15 (drums: 10) */
    uint8_t master_cc;                   /* the MASTER pot: 0 = the synth's volume, else its controller number */
    uint8_t glow;                        /* unlit buttons: 0 off, 1 low, 2 high */
    track_t trk[NTRACKS];
    uint8_t sound;                       /* SOUND_IN on (0, the default) or SOUND_OFF; SOUND_KEYS from older
                                          * projects is read as on with in_mute */
    uint8_t in_mute;                     /* 1: MIDI in does not play the synth */
    uint8_t touch;                       /* velocity from the keys: 0 off (every note proj.vel), 1 on */
    uint8_t at_mode;                     /* AT_*: the keys' aftertouch */
    int8_t vel_curve;                    /* -5..+5: the velocity curve, 0 as measured (+ harder to reach 127) */
    int8_t at_sens;                      /* -5..+5: how far a lean goes for full aftertouch, 0 as measured */
    uint8_t rsv[10];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
uint16_t euclid(int steps, int hits, int rot);

/* the engine (engine.c): engine_tick from the platform's 1 kHz timer (an interrupt on the device), the
 * rest from the main loop */
void engine_tick(void);
void engine_midi_clock(uint32_t status); /* F8 FA FB FC from MIDI in (followed when the clock is external) */
void engine_play(int on);                /* PLAY */
void engine_reset_tempo(void);
void arp_press(int note);
void arp_release(int note);
void arp_set(int on);                    /* on / off; off ends the note */
void arp_latch_changed(void);
void engine_mod(int up);                 /* LFO: the mod wheel up (held) or down, over a sixteenth */
void engine_panic(void);                 /* every note the engine has on, off */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
