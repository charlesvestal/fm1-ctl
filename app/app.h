/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Ctl app: the settings (everything that is saved), the engine (clock, arpeggiator, sequencer) and its half of the
 * contract with fm1-base (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "gm.h"

#define APP_NAME "CTL-1"
#define APP_TAGLINE "A MIDI controller for the FM-1"
#define APP_BLOCK 256u                   /* the GM synth's block: 5.8 ms at 44.1 kHz */

/* the store: the settings at 0xFC000 (Felucca's FL_GLOB), the panel calibration at 0xDC000, the Bluetooth stack's
 * syscfg (its address) at 0xDE000 */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u}

/* MIDI routing beside plat.h's ports: Ctl sends through ctl_send (app.c), which also plays the onboard synth */
#define TO_SYNTH (1u << 4)               /* the onboard GM synth (when Sound is on) */
#define TO_NOSYNTH (1u << 5)             /* MIDI thru: not to the synth (it gets MIDI in by TO_SYNTH, if asked) */
void ctl_send(uint32_t pkt, uint32_t to);   /* to: TO_USB | TO_BLE (and TO_SYNTH, TO_NOSYNTH); main loop or the tick.
                                          * Ctl's own notes (to USB or BLE) also play the synth */
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
    uint8_t ble_off;                     /* Bluetooth switched off (Setup, Bluetooth page); 0, the default: on */
    uint8_t rsv[9];                      /* room to grow */
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

/* the contract with fm1-base: the platform calls these (app.c) */
void app_init(void);                     /* boot: the settings, then the GM synth */
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12);   /* audio interrupt: the GM synth */
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
#define APP_GAIN(m) (proj.master_cc ? 4096u : (m) * (m) >> 12)   /* MASTER sends a controller: full; else the volume */
#define APP_BLE_AT_BOOT (!proj.ble_off)  /* Bluetooth at power-on: as the player left it */
#define APP_TICK                         /* the clock, arpeggiator and sequencer (engine_tick) */
void app_tick(void);
#define APP_QUIET                        /* the radio's first second: the synth keeps time, silent (gm_skip) */
void app_quiet(int32_t *lr, uint32_t n);
#define APP_OVERLOAD                     /* the synth sheds voices */
void app_overload(void);
#define APP_SPLASH                       /* the name on paper, underlined */
void app_splash(void);
#define APP_TOUCH                        /* velocity and aftertouch from the keys (fm1-base's lib/touch.h) */

/* the host simulator (host.c): its checks, commands, a reboot, the screen reader's dump */
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#define APP_HOST_REBOOT
void app_host_reboot(void);
#define APP_HOST_SPEAKDUMP
void app_speak_dump(void);
