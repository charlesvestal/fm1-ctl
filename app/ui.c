/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Ctl UI: a MIDI controller. Part of the unity build (after gfx.c, project.c and engine.c); the host
 * simulator includes it too.
 *
 *   keys          chromatic from F3; with a scale: white keys the scale's steps, black keys its chords
 *   KNOB 1-4      the page's four values (on Play: the knob bank's four controllers)
 *   SELECT the page's pages (Play: the banks; Sequencer: the tracks)  ALGORITHM MIDI channel
 *   PRESETS program change  (the tempo: Setup, KNOB 1)
 *   FX            the next knob bank (A..D); held on Play, KNOB 1-4 set the bank's controller numbers
 *   ARP           the Arp page (KNOB 1 there turns it on: Off / On / Latch)
 *   PLAY          start / stop (the sequencer, MIDI start / stop)    REC  panic (all notes off)
 *   ENV           sustain while held; a double tap locks it on (a press unlocks)    OCT- / OCT+  the octave (-3..+3)
 *   LFO           the mod wheel (CC 1): up over a sixteenth while held, back down after
 *   pages         one button each: HOME Play, SEL Keys, ARP Arp, SEQ Sequencer, EDIT Sound, GLO Setup
 *   SAVE          saves (it also saves by itself, a few seconds after a change)
 *
 * Touch (SELECT on Setup): velocity and aftertouch from the keys themselves (MONK-1's sensing, from Velo:
 * sense.c), each optional and off by default. On, the 20 keys on the matrix's analog rows (F3..C5) play
 * when sense.c has their velocity (a few ms after the contact), the seven above them (the PA6 row, no
 * converter) at the digital contact with the PA10 probe's velocity; aftertouch per note or the channel's.
 *
 * On the Sequencer page the keys edit instead of playing: white keys the selected track's 16 steps,
 * black keys 1-4 select a track, 5-8 mute one.
 *
 * Out: everything played goes to USB and Bluetooth. Thru: USB in -> Bluetooth, Bluetooth in -> USB,
 * the TRS jack in -> USB and Bluetooth. The USB-only build (FM1B_BLE 0) shows no Bluetooth. */

enum { V_PLAY, V_SCALE, V_ARP, V_SEQ, V_SOUND, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Play", "Keys", "Arp", "Sequencer", "Sound", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEL, B_ARP, B_SEQ, B_EDIT, B_GLO};
static const char *const PORT_NAME[NPORTS] = {"USB", "BLE", "TRS"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const SCALE_NAME[NSCALES] = {"Chromatic", "Major", "Minor", "Dorian", "Mixolydian",
                                                "Harm. minor", "Pentatonic", "Pent. minor", "Blues"};
static const uint8_t SCALE_LEN[NSCALES] = {12, 7, 7, 7, 7, 7, 5, 5, 6};
static const uint8_t SCALE_IV[NSCALES][7] = {{0}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 9, 10},
                                             {0, 2, 4, 5, 7, 9, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 4, 7, 9},
                                             {0, 3, 5, 7, 10}, {0, 3, 5, 6, 7, 10}};
static const char *const ARP_NAME[NARP] = {"Up", "Down", "Up-down", "Random", "As played"};
static const char *const RATE_NAME[NRATE] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const CLOCK_NAME[NCLK] = {"Int", "Int, quiet", "Ext"};
static const char *const GLOW_NAME[3] = {"Off", "Low", "High"};
static const char *const AT_NAME[NAT] = {"Off", "Poly", "Channel"};
#include "gm_names.h"
#include "fastmath.h"
#define KEY0_NOTE 53                   /* chromatic: F3 is key 0 at octave 0 */
#define SCALE0_NOTE 48                 /* a scale: its root from C3 at octave 0 */
#define FLASH_MS 120u                  /* a port's light after a message */

/* the FM-1's keys: 16 white, 11 black, as the keyboard runs F3..G5 */
static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* the screen's colours, paper and ink: warm white paper, ink-blue type, vermilion for what is played,
 * mustard for controllers and connections (_T: their tints; K_COOL_D: mustard dark enough for type) */
#define K_BG RGB(251, 248, 241)
#define K_PANEL RGB(239, 233, 220)
#define K_LINE RGB(221, 212, 194)
#define K_DIM RGB(122, 128, 152)
#define K_TEXT RGB(30, 43, 90)
#define K_WHITE RGB(255, 255, 255)
#define K_HOT RGB(229, 72, 46)
#define K_HOT_T RGB(248, 211, 201)
#define K_COOL RGB(217, 162, 27)
#define K_COOL_T RGB(245, 228, 184)
#define K_COOL_D RGB(158, 112, 6)
#define K_KEYW RGB(255, 253, 248)
#define K_KEYB RGB(30, 43, 90)

enum { KM_NONE, KM_NOTES, KM_ARP };    /* what a held key did: sent notes, gave them to the arpeggiator */
static struct {
    uint8_t view;
    uint8_t setup_pg;                  /* Setup's pages (SELECT): 0 the settings, 1 Touch, 2 Bluetooth */
    uint8_t last_vel;                  /* the last note's velocity from the keys (Touch shows it) */
    uint8_t track;                     /* the sequencer track the Sequencer page edits */
    uint32_t btn, keys;
    uint32_t enc_t[NE];
    uint8_t kmode[NKEYS], knotes[NKEYS][3], kn[NKEYS];   /* per held key: how, which notes */
    uint32_t heard[4];                 /* notes held from MIDI in (any port), for the key lights */
    uint8_t playing;
    uint8_t sustain;
    uint8_t sus_lock;                  /* ENV tapped twice: sustain stays on after the release */
    uint32_t env_t;                    /* when ENV last went down */
    uint32_t save_t;                   /* when SAVE went down; save_long: held long enough (Setup: Bluetooth) */
    uint8_t save_long;
    uint8_t fx_used;                   /* a knob turned while FX was held: no bank change at its release */
    uint32_t fx_t;                     /* when FX went down */
    uint8_t dirty;
    uint32_t act_t;
    int32_t master_raw;                /* the MASTER pot, smoothed, and the value last sent */
    int16_t master_sent;
    char last[24];                     /* the last message played here, in words */
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[4];                   /* what each band shows: redrawn when it changes */
} ui;

/* the screen reader (announce.c; the "screen reader" section below): what the key handlers did, said at the
 * next frame (text is built there, not in the handlers) */
#ifdef PLAT_HOST
#define SP_POOL
#else
#define SP_POOL __attribute__((section(".pool")))
#endif
enum { SE_NONE, SE_STEP, SE_STEP_OUT, SE_MUTE, SE_PANIC };
static struct {
    uint32_t ctx;                      /* the page, hashed: a change is said */
    int16_t val[4];                    /* the four knobs' values */
    uint8_t chan, program, playing, sus_lock, ble, started;
    uint32_t touch_until;              /* a knob turned (even when its value could not change) */
    uint8_t ev, ev_arg, ev_on;         /* the last key event: SE_* */
} sp SP_POOL;
static uint8_t sp_said;                /* a message went to the screen reader this frame: speak() leaves it be */
static void sp_event(int ev, int arg, int on)
{
    sp.ev = (uint8_t)ev;
    sp.ev_arg = (uint8_t)arg;
    sp.ev_on = (uint8_t)on;
}

static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader says it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message on the top bar; flags: how a screen reader gets it (0: not at all, the page change says it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }
int ui_dirty(void) { return ui.dirty != 0; }
static void mark_dirty(void) { ui.dirty = 1; }

static char *put_s(char *b, const char *s)
{
    while (*s)
        *b++ = *s++;
    *b = 0;
    return b;
}
static char *put_u(char *b, uint32_t v)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}
static char *put_note(char *b, int n)    /* C4 = 60 */
{
    b = put_s(b, NOTE_NAME[n % 12]);
    if (n < 12)
        return put_s(b, "-1");
    return put_u(b, (uint32_t)(n / 12 - 1));
}

/* a program by name while the synth is on (the drum channel: the kit), else its number */
static char *prog_name(char *b, uint32_t p)
{
    if (proj.sound == SOUND_OFF)
        return put_u(put_s(b, "Program "), p + 1u);
    return put_s(b, proj.chan == 9 ? "Drum kit" : GM_PROGRAM[p & 127u]);
}

/* ------------------------------------------------------------- MIDI --- */
static uint32_t pkt3(uint32_t st, uint32_t d1, uint32_t d2) { return (st >> 4) | st << 8 | d1 << 16 | d2 << 24; }
/* the onboard synth on the program shown (only there: the program goes out over MIDI when PRESETS turns) */
static void synth_program(void) { ctl_send(pkt3(0xC0u | proj.chan, proj.program, 0), TO_SYNTH); }

static void send(uint32_t pkt, uint32_t to) { ctl_send(pkt, to); }   /* (fm1-base counts each port's messages) */
static void play(uint32_t st, uint32_t d1, uint32_t d2) { send(pkt3(st | proj.chan, d1, d2), TO_USB | TO_BLE); }

static void cc(uint32_t num, uint32_t v)
{
    char *b;
    play(0xB0u, num, v);
    b = put_u(put_s(ui.last, "CC "), num);
    put_u(put_s(b, " = "), v);
}

/* the notes a key plays: chromatic, or with a scale the white keys its steps and the black keys the
 * triads on its steps */
static int key_white(int k)
{
    int i;
    for (i = 0; i < 16; i++)
        if (WHITE_K[i] == k)
            return i;
    return -1;
}
static int key_black(int k)
{
    int i;
    for (i = 0; i < 11; i++)
        if (BLACK_K[i] == k)
            return i;
    return -1;
}
static int scale_note(int deg)
{
    int len = SCALE_LEN[proj.scale];
    return SCALE0_NOTE + proj.root + 12 * proj.octave + 12 * (deg / len) + SCALE_IV[proj.scale][deg % len];
}
static int key_notes(int k, uint8_t *out)
{
    int n = 0, i, w, v;
    if (!proj.scale) {
        v = KEY0_NOTE + k + 12 * proj.octave;
        if (v >= 0 && v < 128)
            out[n++] = (uint8_t)v;
        return n;
    }
    if ((w = key_white(k)) >= 0) {
        v = scale_note(w);
        if (v >= 0 && v < 128)
            out[n++] = (uint8_t)v;
        return n;
    }
    w = key_black(k);
    for (i = 0; i < 3; i++) {
        v = scale_note(w + 2 * i);
        if (v >= 0 && v < 128)
            out[n++] = (uint8_t)v;
    }
    return n;
}

static void key_down(int k, uint32_t vel)
{
    uint8_t *q = ui.knotes[k];
    int n = key_notes(k, q), i;
    char *b;
    ui.kn[k] = (uint8_t)n;
    if (!n)
        return;
    ui.kmode[k] = proj.arp_on ? KM_ARP : KM_NOTES;
    for (i = 0; i < n; i++) {
        if (proj.arp_on)
            arp_press(q[i]);
        else
            play(0x90u, q[i], vel);
    }
    b = put_note(put_s(ui.last, n > 1 ? "Chord " : "Note "), q[0]);
    if (!proj.arp_on)
        put_u(put_s(b, "  "), vel);
}

static void key_up(int k)
{
    int i;
    for (i = 0; i < ui.kn[k]; i++) {
        if (ui.kmode[k] == KM_ARP)
            arp_release(ui.knotes[k][i]);
        else if (ui.kmode[k] == KM_NOTES)
            play(0x80u, ui.knotes[k][i], 0);
    }
    ui.kmode[k] = KM_NONE;
    ui.kn[k] = 0;
}

static void all_keys_up(void)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        key_up(k);
}

static void panic(void)
{
    all_keys_up();
    engine_panic();
    play(0xB0u, 64, 0);
    play(0xB0u, 123, 0);                 /* all notes off */
    if (proj.seq_ch != proj.chan)
        send(pkt3(0xB0u | proj.seq_ch, 123, 0), TO_USB | TO_BLE);
    ui.sustain = ui.sus_lock = 0;
    put_s(ui.last, "All notes off");
}

/* MIDI in: forwarded (USB -> BLE, BLE -> USB, TRS -> both), noted for the lights; clock and
 * start / stop drive the engine when the clock is external */
/* a controller heard on the channel: the knobs set to it show its value (a device that sends back, as the
 * OXI E16, stays in step). It counts as activity: the autosave waits for the device to rest too (each save stops
 * everything for a moment, and the lights flickered while a device's knob turned) */
static void follow_cc(uint32_t num, uint32_t v)
{
    uint32_t b, k;
    for (b = 0; b < NBANKS; b++)
        for (k = 0; k < NKNOBS; k++)
            if (proj.cc[b][k] == num && proj.val[b][k] != v) {
                proj.val[b][k] = (uint8_t)v;
                mark_dirty();
                ui.act_t = plat_ms();
            }
}

static const uint32_t THRU[NPORTS] = {TO_BLE, TO_USB, TO_USB | TO_BLE};
static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (port < 0 || port >= NPORTS)
            continue;
        send(pkt & ~0xF0u, THRU[port] | TO_NOSYNTH);  /* cable 0 */
        if (!proj.in_mute)
            send(pkt & ~0xF0u, TO_SYNTH);
        if (st >= 0xF8u) {
            engine_midi_clock(st);
            if (proj.clock == CLK_EXT && (st == 0xFAu || st == 0xFBu || st == 0xFCu))
                ui.playing = st != 0xFCu;
        } else if ((st & 0xF0u) == 0x90u && v) {
            ui.heard[n >> 5] |= 1u << (n & 31u);
        } else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) {
            ui.heard[n >> 5] &= ~(1u << (n & 31u));
        } else if ((st & 0xF0u) == 0xB0u && (n == 123u || n == 120u)) {
            memset(ui.heard, 0, sizeof ui.heard);
        }
        if ((st & 0xF0u) == 0xB0u && (st & 0x0Fu) == proj.chan)
            follow_cc(n, v);
    }
}

/* ------------------------------------------------------------ touch --- */
/* velocity and aftertouch from the keys (fm1-base's lib/touch.h: MONK-1's sensing and pressure, with the
 * settings tuned on CTL-1). The scan samples the analog rows only while one of them is on. */
#define touch_live touch_is_on()       /* the scan is sampling (plat_sense) */
static int touch_wanted(void) { return proj.touch || proj.at_mode != AT_OFF; }

/* the sampling on or off as Touch asks; the keys change hands (touch.c or the digital edge): all up */
static void touch_apply(void)
{
    int want = touch_wanted();
    touch_curve(proj.vel_curve);
    touch_sens(proj.at_sens);
    if (want == touch_live)
        return;
    all_keys_up();
    touch_on(want);
}

/* aftertouch, as touch.c measures each key's pressure (every 5 ms): poly to the key's notes, or the
 * hardest-pressed key's as channel pressure */
static uint8_t at_sent[NKEYS], at_chan;
static uint32_t at_t;

static void at_off(void)               /* aftertouch back to 0 where it was sent */
{
    int k, i;
    for (k = 0; k < NKEYS; k++) {
        if (at_sent[k] && proj.at_mode == AT_POLY && ui.kmode[k] == KM_NOTES)
            for (i = 0; i < ui.kn[k]; i++)
                play(0xA0u, ui.knotes[k][i], 0);
        at_sent[k] = 0;
    }
    if (at_chan)
        play(0xD0u, 0, 0);
    at_chan = 0;
}

static void pressure(void)
{
    uint32_t i, now = plat_ms();
    uint8_t top = 0;
    touch_update();
    if (proj.at_mode == AT_OFF || now - at_t < 5u)
        return;
    at_t = now;
    for (i = 0; i < NKEYS; i++) {
        uint8_t p;
        int j;
        if (!touch_sensed(i))
            continue;
        if (!touch_down(i) || ui.kmode[i] == KM_NONE) {
            at_sent[i] = 0;
            continue;
        }
        p = touch_pressure(i);
        if (p > top)
            top = p;
        if (p != at_sent[i]) {
            at_sent[i] = p;
            if (proj.at_mode == AT_POLY && ui.kmode[i] == KM_NOTES)   /* the arpeggiator's notes: no poly AT */
                for (j = 0; j < ui.kn[i]; j++)
                    play(0xA0u, ui.knotes[i][j], p);
        }
    }
    if ((proj.at_mode == AT_CHAN || proj.arp_on) && top != at_chan) {   /* the hardest-pressed key's */
        at_chan = top;
        play(0xD0u, top, 0);
    }
}

/* the analog keys' notes, as touch.c finds them (off the Sequencer page: there the keys edit, at the contact) */
static void touch_events(void)
{
    touch_ev_t e;
    if (!touch_live)
        return;
    while (touch_take(&e)) {
        if (ui.view == V_SEQ)
            continue;
        if (e.type == TOUCH_PRESS) {
            uint32_t v = proj.touch ? e.vel : proj.vel;
            ui.last_vel = e.vel;
            key_up(e.key);                       /* a retrigger (the key bounced back open) */
            key_down(e.key, v);
        } else {
            key_up(e.key);
        }
    }
    pressure();
}

/* a key without a converter (or every key, Touch off): the velocity at its contact */
static uint32_t edge_vel(uint32_t k)
{
    uint32_t v;
    if (touch_live && proj.touch && (v = touch_vel_contact(k)) != 0) {
        ui.last_vel = (uint8_t)v;
        return v;
    }
    return proj.vel;
}

/* ------------------------------------------------------------ values --- */
/* what a knob edits: one of these */
enum { K_VAL0, K_VAL1, K_VAL2, K_VAL3, K_CC0, K_CC1, K_CC2, K_CC3, K_ROOT, K_SCALE, K_VEL, K_HUMAN,
       K_AMODE, K_ARATE, K_AOCT, K_ARP, K_STEPS, K_HITS, K_ROT, K_NOTE, K_CLOCK, K_SEQCH, K_MASTER, K_GLOW,
       K_TEMPO, K_SOUND, K_SNDIN, K_TOUCH, K_CURVE, K_ATM, K_ATS, K_DEV, K_BT, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {K_VAL0, K_VAL1, K_VAL2, K_VAL3},
    {K_ROOT, K_SCALE, K_VEL, K_HUMAN},
    {K_ARP, K_AMODE, K_ARATE, K_AOCT},
    {K_STEPS, K_HITS, K_ROT, K_NOTE},
    {K_SOUND, K_SNDIN, K_MASTER, K_NONE},
    {K_TEMPO, K_CLOCK, K_SEQCH, K_GLOW},
};
static const uint8_t TOUCH_KNOB[4] = {K_TOUCH, K_CURVE, K_ATM, K_ATS};   /* Setup's second page */
/* Setup's third page (FM1B_BLE): a Bluetooth MIDI device (FM1B_BLE_CENTRAL), and Bluetooth on or off */
static const uint8_t BT_KNOB[4] = {FM1B_BLE_CENTRAL ? K_DEV : K_NONE, K_NONE, K_NONE, K_BT};
#define SETUP_PAGES (FM1B_BLE ? 3 : 2)
/* Bluetooth in a word, for the ports, Setup and a screen reader: "off" (by the player), "stopped" (after a crash:
 * SAVE starts it), "starting", "failed", "connected" (a computer, or a device), "ready" */
static int bt_stopped(void) { return FM1B_BLE && !proj.ble_off && plat_ble_state() == BLE_OFF; }
static const char *bt_status(void)
{
    int s = plat_ble_state();
    if (proj.ble_off)
        return "off";
    if (s == BLE_OFF)
        return "stopped";
    if (s == BLE_STARTING)
        return "starting";
    if (s == BLE_FAILED)
        return "failed";
    return plat_ble_linked() || plat_ble_device() == BLE_DEV_READY ? "connected" : "ready";   /* (linked: it holds
                                                                                                * the FM-1, listening or not) */
}
/* FX held on Play: the knobs set the bank's controller numbers, shown once it has been held a moment
 * (a tap to change banks does not flash them) or a knob turns */
#define FX_HOLD_MS 400u
static int fx_cc(void)
{
    return ui.view == V_PLAY && (ui.btn >> B_FX & 1u) && (ui.fx_used || plat_ms() - ui.fx_t >= FX_HOLD_MS);
}
static int view_knob(int i)
{
    if (fx_cc())
        return K_CC0 + i;
    if (ui.view == V_SETUP && ui.setup_pg == 2)   /* (a device only while Bluetooth runs) */
        return i == 0 && (proj.ble_off || plat_ble_state() != BLE_READY) ? K_NONE : BT_KNOB[i];
    if (ui.view == V_SETUP && ui.setup_pg)
        return TOUCH_KNOB[i];
    return VIEW_KNOB[ui.view][i];
}

static track_t *trk(void) { return &proj.trk[ui.track]; }
static void track_name(int n, char *b);

static int knob_get(int k)
{
    if (k <= K_VAL3)
        return proj.val[proj.bank][k - K_VAL0];
    if (k <= K_CC3)
        return proj.cc[proj.bank][k - K_CC0];
    switch (k) {
    case K_ROOT: return proj.root;
    case K_SCALE: return proj.scale;
    case K_VEL: return proj.vel;
    case K_HUMAN: return proj.human;
    case K_AMODE: return proj.arp_mode;
    case K_ARATE: return proj.arp_rate;
    case K_AOCT: return proj.arp_oct;
    case K_ARP: return proj.arp_on ? 1 + proj.arp_latch : 0;
    case K_STEPS: return trk()->steps;
    case K_HITS: return trk()->hits;
    case K_ROT: return trk()->rot;
    case K_NOTE: return trk()->note;
    case K_CLOCK: return proj.clock;
    case K_SEQCH: return proj.seq_ch;
    case K_MASTER: return proj.master_cc;
    case K_GLOW: return proj.glow;
    case K_TEMPO: return proj.tempo;
    case K_SOUND: return proj.sound != SOUND_OFF;
    case K_SNDIN: return !proj.in_mute;
    case K_TOUCH: return proj.touch;
    case K_CURVE: return proj.vel_curve;
    case K_ATM: return proj.at_mode;
    case K_ATS: return proj.at_sens;
    case K_DEV: return btdev_pick();
    case K_BT: return !proj.ble_off;
    default: return 0;
    }
}
static void knob_range(int k, int *lo, int *hi)
{
    *lo = 0;
    switch (k) {
    case K_ROOT: *hi = 11; break;
    case K_SCALE: *hi = NSCALES - 1; break;
    case K_VEL: *lo = 1; *hi = 127; break;
    case K_HUMAN: *hi = 40; break;
    case K_AMODE: *hi = NARP - 1; break;
    case K_ARATE: *hi = NRATE - 1; break;
    case K_AOCT: *lo = 1; *hi = 3; break;
    case K_SOUND: case K_SNDIN: case K_TOUCH: case K_BT: *hi = 1; break;
    case K_DEV: *hi = btdev_count() > 1 ? btdev_count() - 1 : 0; break;
    case K_CURVE: case K_ATS: *lo = -5; *hi = 5; break;
    case K_ATM: *hi = NAT - 1; break;
    case K_TEMPO: *lo = 40; *hi = 240; break;
    case K_ARP: *hi = 2; break;
    case K_STEPS: *lo = 1; *hi = 16; break;
    case K_HITS: *hi = trk()->steps; break;
    case K_ROT: *hi = trk()->steps - 1; break;
    case K_CLOCK: *hi = NCLK - 1; break;
    case K_SEQCH: *hi = 15; break;
    case K_GLOW: *hi = 2; break;
    case K_NOTE: case K_VAL0: case K_VAL1: case K_VAL2: case K_VAL3: *hi = 127; break;
    default: *hi = 119; break;           /* controller numbers, MASTER */
    }
}
static void knob_name(int k, char *b)
{
    static const char *const N[] = {"Root", "Scale", "Velocity", "Humanize", "Mode", "Rate", "Octaves", "Arp",
                                    "Steps", "Hits", "Rotate", "Note", "Clock", "Seq ch", "MASTER", "Glow",
                                    "Tempo", "Sound", "MIDI in", "Velocity", "Curve", "Aftertouch", "AT sens", "Device",
                                    "Bluetooth"};
    if (k <= K_VAL3)
        put_u(put_s(b, "CC "), proj.cc[proj.bank][k - K_VAL0]);
    else if (k <= K_CC3)
        put_u(put_s(b, "Knob "), (uint32_t)(k - K_CC0 + 1));
    else if (k < K_NONE)
        put_s(b, N[k - K_ROOT]);
    else
        b[0] = 0;
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    switch (k) {
    case K_ROOT: put_s(b, NOTE_NAME[v]); break;
    case K_SCALE: put_s(b, SCALE_NAME[v]); break;
    case K_AMODE: put_s(b, ARP_NAME[v]); break;
    case K_ARATE: put_s(b, RATE_NAME[v]); break;
    case K_ARP: put_s(b, v == 2 ? "Latch" : v ? "On" : "Off"); break;
    case K_NOTE: track_name(v, b); break;
    case K_CLOCK: put_s(b, CLOCK_NAME[v]); break;
    case K_SEQCH: put_u(b, (uint32_t)v + 1u); break;
    case K_MASTER:
        if (v)
            put_u(put_s(b, "CC "), (uint32_t)v);
        else
            put_s(b, "Volume");
        break;
    case K_GLOW: put_s(b, GLOW_NAME[v]); break;
    case K_SOUND: put_s(b, v ? "On" : "Off"); break;
    case K_SNDIN: put_s(b, v ? "Plays" : "Ignored"); break;
    case K_TOUCH: put_s(b, v ? "Keys" : "Fixed"); break;
    case K_BT: put_s(b, v ? "On" : "Off"); break;
    case K_DEV:
        if (btdev_busy())
            put_s(b, btdev_name());      /* the one connected, or being connected */
        else if (!btdev_found(v, b, 0))
            put_s(b, plat_ble_device() == BLE_DEV_SCANNING ? "Looking" : "None");
        b[10] = 0;                       /* (the knob row's width) */
        break;
    case K_ATM: put_s(b, AT_NAME[v]); break;
    case K_CURVE: case K_ATS:
        if (v > 0)
            put_u(put_s(b, "+"), (uint32_t)v);
        else if (v < 0)
            put_u(put_s(b, "-"), (uint32_t)-v);
        else
            put_s(b, "0");
        break;
    case K_TEMPO:
        if (proj.clock == CLK_EXT)
            put_s(b, "Ext");
        else
            put_u(b, (uint32_t)v);
        break;
    case K_CC0: case K_CC1: case K_CC2: case K_CC3: put_u(put_s(b, "#"), (uint32_t)v); break;
    default: put_u(b, (uint32_t)v); break;
    }
}

static void track_regen(void)
{
    track_t *t = trk();
    if (t->hits > t->steps)
        t->hits = t->steps;
    if (t->rot >= t->steps)
        t->rot = (uint8_t)(t->steps - 1u);
    t->pat = euclid(t->steps, t->hits, t->rot);
}

static void knob_set(int k, int v)
{
    int lo, hi;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k <= K_VAL3) {
        proj.val[proj.bank][k - K_VAL0] = (uint8_t)v;
        cc(proj.cc[proj.bank][k - K_VAL0], (uint32_t)v);
    } else if (k <= K_CC3) {
        proj.cc[proj.bank][k - K_CC0] = (uint8_t)v;
    } else {
        switch (k) {
        case K_ROOT: all_keys_up(); proj.root = (uint8_t)v; break;
        case K_SCALE: all_keys_up(); proj.scale = (uint8_t)v; break;
        case K_VEL: proj.vel = (uint8_t)v; break;
        case K_HUMAN: proj.human = (uint8_t)v; break;
        case K_AMODE: proj.arp_mode = (uint8_t)v; break;
        case K_ARATE: proj.arp_rate = (uint8_t)v; break;
        case K_AOCT: proj.arp_oct = (uint8_t)v; break;
        case K_ARP:
            if (!v != !proj.arp_on) {
                all_keys_up();
                arp_set(v != 0);
            }
            if (v) {
                proj.arp_latch = (uint8_t)(v == 2);
                arp_latch_changed();
            }
            break;
        case K_STEPS: trk()->steps = (uint8_t)v; track_regen(); break;
        case K_HITS: trk()->hits = (uint8_t)v; track_regen(); break;
        case K_ROT: trk()->rot = (uint8_t)v; track_regen(); break;
        case K_NOTE: trk()->note = (uint8_t)v; break;
        case K_CLOCK: proj.clock = (uint8_t)v; break;
        case K_SEQCH: engine_panic(); proj.seq_ch = (uint8_t)v; break;
        case K_MASTER: proj.master_cc = (uint8_t)v; ui.master_sent = -1; break;
        case K_GLOW: proj.glow = (uint8_t)v; break;
        case K_SOUND:
            if (!v)
                gm_panic();
            proj.sound = (uint8_t)(v ? SOUND_IN : SOUND_OFF);
            break;
        case K_SNDIN:
            if (!v)
                gm_panic();              /* notes from MIDI in may be sounding */
            proj.in_mute = (uint8_t)!v;
            break;
        case K_TEMPO:
            if (proj.clock == CLK_EXT)
                return;                  /* following a clock from outside */
            proj.tempo = (uint8_t)v;
            break;
        case K_TOUCH: all_keys_up(); proj.touch = (uint8_t)v; touch_apply(); break;
        case K_ATM: at_off(); proj.at_mode = (uint8_t)v; touch_apply(); break;
        case K_CURVE: proj.vel_curve = (int8_t)v; touch_apply(); break;
        case K_ATS: proj.at_sens = (int8_t)v; break;
        case K_DEV: btdev_choose(v); return;      /* (a choice, not a setting) */
        case K_BT:
            proj.ble_off = (uint8_t)!v;
            plat_ble_enable(v);
            say("BLUETOOTH", v ? "ON" : "OFF");
            break;
        default: return;
        }
    }
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 */
#ifdef PLAT_HOST
#define accel_off plat_host_exact      /* the host simulator's "spin": exact steps */
#else
#define accel_off 0
#endif
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (range <= 24 || !a || accel_off)
        return s;
    if (a > 1)
        dt /= a;
    m = dt < 15u ? 6u : dt < 30u ? 4u : dt < 60u ? 2u : 1u;
    return s * (int32_t)m;
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    if (v == V_SEQ || ui.view == V_SEQ)
        all_keys_up();                   /* the keys change meaning */
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void octave(int d)
{
    char t[8];
    int o = proj.octave + d;
    if (o < -3 || o > 3)
        return;
    proj.octave = (int8_t)o;
    put_note(t, proj.scale ? SCALE0_NOTE + proj.root + 12 * o : KEY0_NOTE + 12 * o);
    say("LOWEST KEY", t);
    mark_dirty();
}

#define ENV_TAP_MS 300u                  /* ENV: two presses this close lock sustain */
#define SAVE_HOLD_MS 1000u               /* SAVE held this long on Setup: Bluetooth by hand */
static void bank_to(int b)                     /* the knob bank, with a word on the top bar */
{
    char t[2] = {(char)('A' + b), 0};
    proj.bank = (uint8_t)b;
    say_as("KNOB BANK", t, 0);                  /* a screen reader hears the page: the bank, its knobs */
    ui.touched = -1;
    mark_dirty();
}

static void button_down(int b)
{
    switch (b) {
    case B_ENV: {                        /* a second press within ENV_TAP_MS locks; a press when locked unlocks */
        uint32_t now = plat_ms();
        if (ui.sus_lock) {
            ui.sus_lock = 0;             /* held now as usual: off at the release */
        } else if (now - ui.env_t < ENV_TAP_MS) {
            ui.sus_lock = 1;
            say("SUSTAIN", "LOCKED");
        }
        ui.env_t = ui.sus_lock ? 0u : now;   /* not a third tap */
        if (!ui.sustain)
            cc(64, 127);
        ui.sustain = 1;
        break;
    }
    case B_LFO: engine_mod(1); break;
    case B_PLAY:
        ui.playing = (uint8_t)!ui.playing;
        engine_play(ui.playing);
        put_s(ui.last, ui.playing ? "Start" : "Stop");
        break;
    case B_REC: panic(); sp_event(SE_PANIC, 0, 0); break;
    case B_OCTDN: octave(-1); break;
    case B_OCTUP: octave(1); break;
    case B_SAVE:
        ui.save_t = plat_ms();
        ui.save_long = 0;
        break;
    case B_FX:
        set_view(V_PLAY);                /* held: the knobs set the controller numbers; a tap: the next bank */
        ui.fx_used = 0;
        ui.fx_t = plat_ms();
        break;
    default: break;
    }
}

static void button_up(int b)
{
    switch (b) {
    case B_ENV:
        if (ui.sus_lock)
            break;
        ui.sustain = 0;
        cc(64, 0);
        break;
    case B_LFO: engine_mod(0); break;
    case B_FX:
        if (!ui.fx_used)
            bank_to((proj.bank + 1) % NBANKS);
        ui.touched = -1;
        break;
    case B_HOME: set_view(V_PLAY); break;
    case B_SEL: set_view(V_SCALE); break;
    case B_SEQ: set_view(V_SEQ); break;
    case B_ARP: set_view(V_ARP); break;
    case B_EDIT: set_view(V_SOUND); break;
    case B_GLO: set_view(V_SETUP); break;
    case B_SAVE:
        if (ui.save_long)
            break;                       /* held: started Bluetooth, no save */
        if (ui.view == V_SETUP && ui.setup_pg == 2 && bt_stopped()) {   /* stopped after a crash: start it */
            plat_ble_start();
            say("BLUETOOTH", "STARTING");
            break;
        }
        if (FM1B_BLE_CENTRAL && ui.view == V_SETUP && ui.setup_pg == 2 && plat_ble_state() == BLE_READY &&
            !proj.ble_off) {                                                /* a device: connect, or let it go */
            int ev = btdev_save();
            say(btdev_word(ev), ev == BTDEV_NO_DEVICE ? 0 : btdev_name());
            break;
        }
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* the Sequencer page: the keys edit */
static void seq_key(int k)
{
    int w = key_white(k), b = key_black(k);
    if (w >= 0) {
        if (w < trk()->steps) {
            trk()->pat ^= (uint16_t)(1u << w);
            mark_dirty();
            sp_event(SE_STEP, w, trk()->pat >> w & 1u);
        } else {
            sp_event(SE_STEP_OUT, w, 0);         /* past the track's end: nothing (a screen reader says so) */
        }
    } else if (b >= 0 && b < 4) {
        ui.track = (uint8_t)b;
        ui.touched = -1;
    } else if (b >= 4 && b < 8) {
        proj.trk[b - 4].mute = (uint8_t)!proj.trk[b - 4].mute;
        mark_dirty();
        sp_event(SE_MUTE, b - 4, proj.trk[b - 4].mute);
    }
}

/* the MASTER pot as a controller: smoothed, sent when it moves by more than its noise */
static void master_pot(void)
{
    int32_t raw = (int32_t)plat_master(), v;
    ui.master_raw += (raw - ui.master_raw) / 4;
    if (!proj.master_cc)                 /* 0: the synth's volume (main_fm1.c) */
        return;
    v = ui.master_raw * 127 / 4000;
    if (v > 127)
        v = 127;
    if (ui.master_sent < 0 || (v != ui.master_sent && (v - ui.master_sent > 1 || ui.master_sent - v > 1 ||
                                                       v == 0 || v == 127))) {
        ui.master_sent = (int16_t)v;
        cc(proj.master_cc, (uint32_t)v);
    }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = keys ^ ui.keys;                 /* keys first: a note is the most urgent thing here */
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            if (ui.view != V_SEQ && touch_live && touch_sensed(i))
                continue;                        /* touch.c plays it (touch_events) */
            if (keys >> i & 1u) {
                if (ui.view == V_SEQ)
                    seq_key((int)i);
                else
                    key_down((int)i, edge_vel(i));
            } else {
                key_up((int)i);
            }
        }
    touch_events();
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++)
        if (ch >> i & 1u) {
            if (btn >> i & 1u)
                button_down((int)i);
            else
                button_up((int)i);
        }
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    master_pot();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* the page's pages, as X0X (the dots on the knob row) */
        int d = e > 0 ? 1 : -1;
        if (ui.view == V_PLAY && proj.bank + d >= 0 && proj.bank + d < NBANKS) {
            bank_to(proj.bank + d);
        } else if (ui.view == V_SEQ && ui.track + d >= 0 && ui.track + d < NTRACKS) {
            ui.track = (uint8_t)(ui.track + d);
            ui.touched = -1;
        } else if (ui.view == V_SETUP && ui.setup_pg + d >= 0 && ui.setup_pg + d < SETUP_PAGES) {
            ui.setup_pg = (uint8_t)(ui.setup_pg + d);
            ui.touched = -1;
        }
    }
    if (FM1B_BLE && ui.view == V_SETUP && (btn >> B_SAVE & 1u) && !ui.save_long &&
        plat_ms() - ui.save_t >= SAVE_HOLD_MS) {  /* SAVE held on Setup: Bluetooth, off after a crash, by hand */
        ui.save_long = 1;
        if (plat_ble_state() == BLE_OFF) {
            plat_ble_start();
            say("BLUETOOTH", "STARTING");
        }
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        int c = clamp(proj.chan + e, 0, 15);
        if (c != proj.chan) {
            all_keys_up();               /* held notes end on the channel they started on */
            engine_panic();
            proj.chan = (uint8_t)c;
            synth_program();
            mark_dirty();
        }
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        int p = clamp(proj.program + e, 0, 127);
        if (p != proj.program) {
            proj.program = (uint8_t)p;
            play(0xC0u, (uint32_t)p, 0);
            prog_name(put_s(put_u(ui.last, (uint32_t)p + 1u), " "), p);
            mark_dirty();
        }
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            int k, lo, hi;
            if (ui.btn >> B_FX & 1u)
                ui.fx_used = 1;
            k = view_knob((int)i);
            if (k != K_NONE) {
                knob_range(k, &lo, &hi);
                knob_set(k, knob_get(k) + accel(EN_K1 + (int)i, e, hi - lo));
            }
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* four bands (X0X's top bar and knob row): the top bar (20 px), the page (120), the status line (18),
 * the knob row (82: its tag on a rule, four knobs, the footer) */
#define HDR_H 20
#define MAIN_Y 20
#define MAIN_H 120
#define STAT_Y 140
#define STAT_H 18
#define KNB_Y 158
#define KNB_H 82

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }
static uint32_t hash_s(uint32_t h, const char *s)
{
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}
static void text_r(int32_t rx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(rx - text_w(f, s), y, f, s, c);
}

/* ---- from X0X: text centred on its capitals, and anti-aliased shapes (coverage 0..16, blended over
 * what is there) */
typedef struct { const felucca_font_t *f; int8_t top, bot; } capm_t;
static capm_t capm[6];
static const capm_t *caps(const felucca_font_t *f)   /* the first and last rows of 'H', measured once */
{
    int i;
    for (i = 0; i < 6 && capm[i].f; i++)
        if (capm[i].f == f)
            return &capm[i];
    if (i == 6)
        i = 5;
    {
        uint32_t gi = ((uint32_t)'H' <= f->last ? 'H' : '0') - f->first, w = f->bw[gi], bpr = (w + 1u) / 2u, x, y;
        const uint8_t *gd = f->data + f->off[gi];
        int top = -1, bot = 0;
        for (y = 0; y < f->h; y++)
            for (x = 0; x < w; x++) {
                uint32_t a = gd[y * bpr + x / 2u];
                a = (x & 1u) ? (a & 15u) : (a >> 4);
                if (a >= 8u) {
                    if (top < 0)
                        top = (int)y;
                    bot = (int)y;
                }
            }
        capm[i].f = f;
        capm[i].top = (int8_t)(top < 0 ? 0 : top);
        capm[i].bot = (int8_t)bot;
    }
    return &capm[i];
}
static int32_t vc(const felucca_font_t *f, int32_t y0, int32_t h)   /* the y that centres f's capitals in [y0, y0+h) */
{
    const capm_t *m = caps(f);
    return y0 + (h - (m->bot - m->top + 1) + 1) / 2 - m->top;
}
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}
static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}
static void dot(int32_t x, int32_t y, int32_t r, uint16_t c) { disc((float)x + 0.5f, (float)y + 0.5f, (float)r + 0.3f, c); }
static float turns(float y, float x)              /* atan2 in turns, 0..1 from +x clockwise (y down) */
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), q = a * a, r;
    r = ((-0.0464964749f * q + 0.15931422f) * q - 0.327622764f) * q * a + a;
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}
/* a knob: a 270-degree track from 7:30 to 4:30, the value filled from the start, round ends and a handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, t <= v ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, v > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, v >= 1.0f ? fill : track);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, K_TEXT);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, K_WHITE);
    }
}
static void play_icon(int32_t x, int32_t y, uint16_t c)   /* a triangle 9 x 11, edges anti-aliased */
{
    int32_t i, j;
    for (j = 0; j < 11; j++)
        for (i = 0; i < 9; i++) {
            float fy = (float)j + 0.5f - 5.5f, fx = (float)i + 0.5f, edge = 8.0f - fx - fm_fabsf(fy) * 1.3f;
            float a = fm_clampf(edge * 0.7f + 0.5f, 0.0f, 1.0f) * fm_clampf(fx + 0.5f, 0.0f, 1.0f);
            px_blend(x + i, y + j, c, (uint32_t)(a * 16.0f + 0.5f));
        }
}

static int32_t tag(int32_t x, int32_t y, const char *s, uint16_t bg, uint16_t fg)   /* a little label */
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(x, y, w, 16, 4, bg);
    cv_text(x + 5, vc(&FONT_XS, y, 16), &FONT_XS, s, fg);
    return w;
}

static int lit(int p, int out) { return plat_midi_ago(p, out) < FLASH_MS; }

/* the top bar (X0X's): the page's name, on Play the knob bank in a chip; the channel in the middle;
 * Bluetooth, the tempo and the play triangle on the right, a dot while unsaved. A message takes the bar. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until, bs = plat_ble_state(), bc = plat_ble_linked(), beat = eng_beat_on();
    int32_t x;
    uint32_t h = hash(hash(hash(2166136261u, ui.view), (uint32_t)msg | ui.dirty << 1 | (uint32_t)bs << 2 |
                                                           (uint32_t)bc << 4 | ui.playing << 5 | proj.arp_on << 6 |
                                                           (uint32_t)beat << 7 | proj.bank << 8),
                      proj.chan | (uint32_t)proj.tempo << 8 | (uint32_t)proj.clock << 16);
    if (msg)
        h = hash(hash_s(hash_s(h, ui.msg[0]), ui.msg[1]), ui.msg_until);
    if (bs == BLE_STARTING)
        h = hash(h, ui.frame / 8u);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    cv_rect(0, HDR_H - 1, 240, 1, K_LINE);
    if (msg) {                                   /* what a button just did: the whole bar */
        int32_t w = text_w(&FONT_B, ui.msg[0]) + 8 + text_w(&FONT_B, ui.msg[1]);
        cv_round(0, 0, 240, HDR_H - 1, 4, K_HOT);
        x = cv_text(120 - w / 2, vc(&FONT_B, 0, HDR_H - 1), &FONT_B, ui.msg[0], K_WHITE);
        cv_text(x + 8, vc(&FONT_B, 0, HDR_H - 1), &FONT_B, ui.msg[1], K_WHITE);
        cv_blit(0, 0);
        return;
    }
    x = cv_text(4, vc(&FONT_B, 1, 17), &FONT_B, VIEW_NAME[ui.view], K_TEXT) + 6;
    if (ui.view == V_PLAY) {                     /* the knob bank, in a chip */
        char b[2] = {(char)('A' + proj.bank), 0};
        int32_t w = text_w(&FONT_B, b) + 10;
        cv_round(x, 1, w, 17, 4, K_COOL);
        cv_text(x + 5, vc(&FONT_B, 1, 17), &FONT_B, b, K_TEXT);
    }
    put_u(put_s(t, "CH "), proj.chan + 1u);
    text_c(132, vc(&FONT_B, 1, 17), &FONT_B, t, K_TEXT);
    if (proj.clock == CLK_EXT)
        put_s(t, "EXT");
    else
        put_u(t, proj.tempo);
    x = 212 - text_w(&FONT_S, t);
    cv_text(x, vc(&FONT_S, 1, 17), &FONT_S, t, proj.clock == CLK_EXT ? K_COOL_D : ui.playing ? K_TEXT : K_DIM);
    play_icon(217, 4, ui.playing ? (beat ? K_HOT : K_TEXT) : K_LINE);
    if (ui.dirty)
        dot(233, 9, 1, K_DIM);
    if (bs != BLE_OFF) {                         /* Bluetooth: connected, waiting, starting (blinks), failed */
        uint16_t c = bc ? K_COOL : bs == BLE_FAILED ? K_HOT_T : bs == BLE_READY ? K_COOL_T :
                     (ui.frame / 8u) & 1u ? K_COOL : K_COOL_T;
        int32_t w = text_w(&FONT_XS, "BLE") + 8;
        x -= w + 6;
        cv_round(x, 3, w, 14, 4, c);
        cv_text(x + 4, vc(&FONT_XS, 3, 14), &FONT_XS, "BLE", bs == BLE_FAILED ? K_HOT : K_TEXT);
    }
    cv_blit(0, 0);
}

/* the keyboard: held keys coral, notes heard teal; with a scale the white keys' roots marked */
static int key_heard(int k)
{
    uint8_t q[3];
    int n = key_notes(k, q);
    return n && (ui.heard[q[0] >> 5] >> (q[0] & 31) & 1u);
}
static int key_root(int k)
{
    int w = key_white(k);
    return proj.scale && w >= 0 && w % SCALE_LEN[proj.scale] == 0;
}
static void draw_keys(int32_t y0, int32_t h)
{
    int i, k;
    cv_round(3, y0 - 1, 16 * 14 + 1, h + 2, 3, K_LINE);
    for (i = 0; i < 16; i++) {
        int32_t x = 4 + i * 14;
        k = WHITE_K[i];
        cv_round(x, y0, 13, h, 2, ui.keys >> k & 1u ? K_HOT : key_heard(k) ? K_COOL_T : K_KEYW);
        if (key_root(k))
            cv_round(x + 4, y0 + h - 9, 5, 5, 2, K_HOT);
    }
    for (i = 0; i < 15; i++) {           /* a black key after each white one that has one */
        int32_t x = 4 + i * 14 + 9;
        k = WHITE_K[i] + 1;
        if (WHITE_K[i + 1] != k + 1)
            continue;
        cv_round(x, y0, 9, h * 3 / 5, 2, ui.keys >> k & 1u ? K_HOT : key_heard(k) ? K_COOL : K_KEYB);
    }
}

static void draw_ports(int32_t y0)
{
    int p, slot = 0;
    for (p = 0; p < NPORTS; p++) {
        int32_t x = 8 + slot * 78;
        if (!FM1B_BLE && p == PORT_BLE)
            continue;                    /* the USB-only build: USB and TRS */
        slot++;
        int i = lit(p, 0), o = p < 2 && lit(p, 1);
        const char *st = "";
        uint16_t sc = K_DIM;
        cv_round(x, y0, 70, 34, 6, K_PANEL);
        cv_text(x + 8, y0 + 2, &FONT_S, PORT_NAME[p], K_TEXT);
        cv_round(x + 52, y0 + 5, 8, 8, 4, i ? K_COOL : K_LINE);   /* in */
        if (p < 2)
            cv_round(x + 52, y0 + 18, 8, 8, 4, o ? K_HOT : K_LINE);   /* out */
        if (p == PORT_BLE) {
            st = bt_status();                /* a computer, or a device */
            sc = st[0] == 'c' ? K_COOL_D : K_DIM;
        } else {
            st = p == PORT_TRS ? "in only" : "";
        }
        cv_text(x + 8, y0 + 18, &FONT_XS, st, sc);
    }
}

static void draw_play(void)
{
    cv_text(12, 4, &FONT_B, ui.last[0] ? ui.last : "Play a key", ui.last[0] ? K_TEXT : K_DIM);
    draw_keys(32, 46);
    draw_ports(86);
}

static void draw_scale(void)
{
    char t[40], *b;
    int i, len = SCALE_LEN[proj.scale];
    if (!proj.scale) {
        cv_text(12, 2, &FONT_B, "Chromatic", K_TEXT);
        cv_text(12, 26, &FONT_XS, "The keys as a keyboard, every note.", K_DIM);
        cv_text(12, 42, &FONT_XS, "KNOB 2 picks a scale.", K_DIM);
        draw_keys(70, 46);
        return;
    }
    b = put_s(t, NOTE_NAME[proj.root]);
    put_s(put_s(b, " "), SCALE_NAME[proj.scale]);
    cv_text(12, 2, &FONT_B, t, K_TEXT);
    b = t;
    for (i = 0; i < len; i++) {
        if (i)
            b = put_s(b, " ");
        b = put_s(b, NOTE_NAME[(proj.root + SCALE_IV[proj.scale][i]) % 12]);
    }
    cv_text(12, 22, &FONT_S, t, K_COOL_D);
    cv_text(12, 42, &FONT_XS, "White keys: the scale. Black keys: its chords.", K_DIM);
    draw_keys(70, 46);
}

static void draw_arp(void)
{
    char t[40], *b;
    uint8_t pool[16];
    int n = eng_pool(pool), i, on = eng_arp_note();
    cv_text(12, 2, &FONT_B, proj.arp_on ? "Arp on" : "Arp off", proj.arp_on ? K_HOT : K_TEXT);
    b = put_s(t, ARP_NAME[proj.arp_mode]);
    b = put_s(put_s(b, "  "), RATE_NAME[proj.arp_rate]);
    b = put_u(put_s(b, "  "), proj.arp_oct);
    b = put_s(b, proj.arp_oct > 1 ? " octaves" : " octave");
    if (proj.arp_latch)
        put_s(b, "  latch");
    cv_text(12, 22, &FONT_XS, t, K_DIM);
    if (!proj.arp_on) {
        cv_text(12, 50, &FONT_XS, "KNOB 1 turns it on; then hold notes or chords.", K_DIM);
        return;
    }
    if (!n)
        cv_text(12, 50, &FONT_XS, "Hold notes or a chord.", K_DIM);
    for (i = 0; i < n; i++) {            /* the notes it plays; the one sounding bright */
        int32_t x = 8 + (i % 8) * 29, y = 46 + (i / 8) * 30;
        int hot = on >= 0 && on % 12 == pool[i] % 12;
        put_note(t, pool[i]);
        cv_round(x, y, 27, 24, 5, hot ? K_HOT : K_HOT_T);
        text_c(x + 13, y + 4, &FONT_XS, t, hot ? K_WHITE : K_TEXT);
    }
}

/* on channel 10, the General MIDI drum names for the common notes */
static const uint8_t DRUM_N[21] = {35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 54, 56, 70, 75};
static int drum_index(int n)                     /* on channel 10, the drum's place in DRUM_N; else -1 */
{
    uint32_t i;
    if (proj.seq_ch == 9)
        for (i = 0; i < sizeof DRUM_N; i++)
            if (DRUM_N[i] == n)
                return (int)i;
    return -1;
}
static void track_name(int n, char *b)
{
    static const char *const S[21] = {"Kick", "Kick", "Rim", "Snare", "Clap", "Snare", "Tom", "Hat", "Tom", "Pedal",
                                      "Tom", "Open", "Tom", "Tom", "Crash", "Tom", "Ride", "Tamb", "Bell", "Shake",
                                      "Clave"};
    int i = drum_index(n);
    if (i >= 0)
        put_s(b, S[i]);
    else
        put_note(b, n);
}

static void draw_seq(void)
{
    int t, s;
    for (t = 0; t < NTRACKS; t++) {
        const track_t *k = &proj.trk[t];
        int32_t y = 5 + t * 30, pos = eng_track_pos(t);
        int sel = t == ui.track;
        char nm[12];
        track_name(k->note, nm);
        if (sel)
            cv_round(2, y - 3, 236, 28, 6, K_PANEL);
        cv_text(6, y + 2, &FONT_XS, nm, k->mute ? K_LINE : sel ? K_TEXT : K_DIM);
        for (s = 0; s < 16; s++) {
            int32_t x = 40 + s * 12;
            int on = k->pat >> s & 1u, cur = s == pos;
            uint16_t c;
            if (s >= k->steps)
                continue;
            c = on ? (k->mute ? K_LINE : cur ? K_TEXT : K_HOT) : cur ? K_DIM : (s % 4 ? K_KEYW : K_HOT_T);
            cv_round(x, y, 11, 22, 3, c);
        }
    }
}

/* Setup's Touch page: what the keys sense, the last velocity and the pressure now */
static void draw_touch(void)
{
    char t[40];
    int32_t w;
    uint32_t k, top = 0;
    cv_text(10, 2, &FONT_S, "Touch", K_TEXT);
    if (!touch_live) {
        cv_text(10, 24, &FONT_XS, "Every note plays at the Velocity", K_DIM);
        cv_text(10, 38, &FONT_XS, "set on Keys. KNOB 1: from the keys.", K_DIM);
        cv_text(10, 58, &FONT_XS, "KNOB 3: aftertouch from the keys.", K_DIM);
        return;
    }
    cv_text(10, 22, &FONT_XS, proj.touch ? "Velocity from the keys" : "Velocity fixed (Keys page)", K_DIM);
    put_s(put_s(t, "Aftertouch "), proj.at_mode == AT_POLY ? "per note" : proj.at_mode == AT_CHAN ? "for the channel" : "off");
    cv_text(10, 36, &FONT_XS, t, K_DIM);
    cv_text(10, 60, &FONT_XS, "Velocity", K_DIM);
    w = 150 * (int32_t)ui.last_vel / 127;
    cv_round(76, 61, 150, 9, 3, K_LINE);
    if (w)
        cv_round(76, 61, w < 6 ? 6 : w, 9, 3, K_HOT);
    for (k = 0; k < NKEYS; k++)
        if (at_sent[k] > top)
            top = at_sent[k];
    cv_text(10, 78, &FONT_XS, "Pressure", K_DIM);
    w = 150 * (int32_t)top / 127;
    cv_round(76, 79, 150, 9, 3, K_LINE);
    if (w)
        cv_round(76, 79, w < 6 ? 6 : w, 9, 3, K_COOL);
    cv_text(10, 100, &FONT_XS, "F3 to C5 sense both; the top seven, velocity.", K_DIM);
}

static int bt_bars(int rssi) { return rssi > -60 ? 3 : rssi > -75 ? 2 : 1; }   /* a device's signal */
static void bt_signal(int32_t x, int32_t y, int bars, uint16_t c)
{
    int i;
    for (i = 0; i < 3; i++)
        cv_rect(x + i * 4, y + 8 - i * 3, 3, 3 + i * 3, i < bars ? c : K_LINE);
}

/* Setup's Bluetooth page: a computer or tablet connecting to the FM-1, and (FM1B_BLE_CENTRAL) the MIDI devices
 * around, the one connected. Off, starting or stopped: a word on what to do instead. (Its log: fm1_remote.py.) */
static void draw_bt(void)
{
    char t[40];
    int d = plat_ble_device(), n = btdev_count(), s = plat_ble_state(), i;
    if (proj.ble_off || s != BLE_READY) {
        const char *a, *b;
        if (proj.ble_off) {
            a = "Bluetooth off";
            b = "KNOB 4 turns it on.";
        } else if (s == BLE_OFF) {
            a = "Bluetooth stopped";
            b = "It stopped after a crash. SAVE starts it.";
        } else if (s == BLE_STARTING) {
            a = "Bluetooth starting";
            b = "A moment.";
        } else {
            a = "Bluetooth failed";
            b = "Turn the FM-1 off and on again.";
        }
        cv_text(10, 2, &FONT_S, a, K_TEXT);
        cv_text(10, 24, &FONT_XS, b, K_DIM);
        return;
    }
    cv_text(10, 2, &FONT_S, "Computer or tablet", K_TEXT);
    if (plat_ble_connected()) {
        text_r(230, 4, &FONT_XS, "connected", K_COOL_D);
    } else if (plat_ble_linked()) {                  /* linked, its MIDI never started: it hides the FM-1 */
        text_r(230, 4, &FONT_XS, "connected", K_COOL_D);
        cv_text(10, 20, &FONT_XS, "No MIDI yet. Mac: reopen Audio MIDI Setup.", K_HOT);
    } else {
        text_r(230, 4, &FONT_XS, "not connected", K_DIM);
        cv_text(10, 20, &FONT_XS, "Connect to \"FM-1\" in its Bluetooth MIDI.", K_DIM);
    }
    if (!FM1B_BLE_CENTRAL)
        return;
    cv_rect(10, 38, 220, 1, K_LINE);
    btdev_title(t);
    cv_text(10, 43, &FONT_S, t, d == BLE_DEV_READY ? K_COOL_D : K_TEXT);
    if (btdev_busy())
        return;
    for (i = 0; i < n && i < 4; i++) {           /* the devices found; KNOB 1's choice marked */
        char name[21];
        int rssi = 0, on = i == btdev_pick();
        plat_ble_found(i, name, &rssi);
        if (on)
            cv_round(6, 61 + i * 14, 228, 13, 4, K_PANEL);
        cv_text(12, 61 + i * 14, &FONT_XS, name, on ? K_TEXT : K_DIM);
        bt_signal(216, 63 + i * 14, bt_bars(rssi), on ? K_TEXT : K_DIM);
    }
    if (!n)
        cv_text(10, 61, &FONT_XS, "Put the device in pairing mode.", K_DIM);
}

static void draw_setup(void)                     /* the settings below; what the ports have carried here */
{
    char t[40], *b;
    int32_t y = 24;
    if (ui.setup_pg == 2) {
        draw_bt();
        return;
    }
    if (ui.setup_pg) {
        draw_touch();
        return;
    }
    cv_text(10, 2, &FONT_S, "MIDI", K_TEXT);
    cv_text(236 - text_w(&FONT_XS, "CTL-1 " APP_VERSION), 4, &FONT_XS, "CTL-1 " APP_VERSION, K_DIM);
    b = put_u(put_s(t, "USB   in "), plat_midi_count(PORT_USB, 0));
    put_u(put_s(b, "   out "), plat_midi_count(PORT_USB, 1));
    cv_text(10, y, &FONT_XS, t, K_DIM);
    if (FM1B_BLE) {
        y += 16;
        if (plat_ble_state() == BLE_OFF || plat_ble_state() == BLE_STARTING) {
            put_s(put_s(t, "Bluetooth   "), bt_stopped() ? "stopped after a crash" : bt_status());
        } else {
            b = put_u(put_s(t, "Bluetooth   in "), plat_midi_count(PORT_BLE, 0));
            put_u(put_s(b, "   out "), plat_midi_count(PORT_BLE, 1));
        }
        cv_text(10, y, &FONT_XS, t, K_DIM);
    }
    put_u(put_s(t, "TRS   in "), plat_midi_count(PORT_TRS, 0));
    cv_text(10, y + 16, &FONT_XS, t, K_DIM);
    cv_text(10, 72, &FONT_XS, "Settings save by themselves; SAVE: now.", K_DIM);
    cv_text(10, 88, &FONT_XS, "OCT- and OCT+ held 5 s: update mode.", K_DIM);
}

static void draw_sound(void)                     /* the onboard GM synth: what it plays, and its load */
{
    char t[40];
    uint32_t p = gm_program[proj.chan];
    if (proj.sound == SOUND_OFF) {
        cv_text(12, 2, &FONT_B, "Off", K_TEXT);
        cv_text(12, 24, &FONT_XS, "KNOB 1 turns the onboard GM sounds on.", K_DIM);
        return;
    }
    cv_text(12, 2, &FONT_B, proj.chan == 9 ? "Drum kit" : GM_PROGRAM[p], K_TEXT);
    if (proj.chan == 9)
        put_s(t, "GM drums, channel 10");
    else
        put_u(put_s(t, "GM program "), p + 1u);
    cv_text(12, 22, &FONT_XS, t, K_DIM);
    cv_text(12, 46, &FONT_XS, proj.in_mute ? "Plays the keys (not MIDI in)" : "Plays the keys and MIDI in", K_DIM);
    cv_text(12, 64, &FONT_XS, "PRESETS picks the sound.", K_DIM);
    put_s(put_u(put_s(put_u(put_s(t, "Voices "), gm_voices), "    CPU "), plat_cpu_pct()), "%");
    cv_text(12, 90, &FONT_XS, t, K_DIM);
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, ui.view), p;
    int i;
    uint8_t pool[16];
    h = hash(h, ui.keys);
    h = hash(hash(h, ui.heard[0] ^ ui.heard[1] * 3u ^ ui.heard[2] * 5u ^ ui.heard[3] * 7u),
             proj.vel | proj.program << 8 | (uint32_t)(proj.octave + 3) << 16 | ui.sustain << 20 | proj.bank << 21);
    h = hash(hash(h, proj.root | proj.scale << 4 | proj.arp_on << 8 | proj.arp_mode << 9 | proj.arp_rate << 12 |
                         proj.arp_oct << 15 | proj.arp_latch << 17 | ui.track << 18),
             (uint32_t)eng_arp_note());
    h = hash(hash_s(h, ui.last), proj.sound | (uint32_t)proj.chan << 4);
    for (p = 0; p < NPORTS; p++)
        h = hash(h, (uint32_t)lit((int)p, 0) | (uint32_t)lit((int)p, 1) << 1);
    h = hash(h, (uint32_t)plat_ble_state() | (uint32_t)plat_ble_connected() << 4 | (uint32_t)plat_ble_device() << 5 |
                    (uint32_t)plat_ble_linked() << 8);
    for (i = 0, p = (uint32_t)eng_pool(pool); i < (int)p; i++)
        h = hash(h, pool[i]);
    if (ui.view == V_SEQ)
        for (i = 0; i < NTRACKS; i++)
            h = hash(hash(h, proj.trk[i].pat | (uint32_t)proj.trk[i].steps << 16 | proj.trk[i].mute << 21 |
                                 (uint32_t)proj.trk[i].note << 22),
                     (uint32_t)eng_track_pos(i));
    if (ui.view == V_SOUND)
        h = hash(hash(h, gm_voices | proj.sound << 8 | proj.in_mute << 12 | (uint32_t)gm_program[proj.chan] << 16),
                 plat_cpu_pct());
    if (ui.view == V_SETUP && ui.setup_pg == 2) {
        char name[21];
        int k, rssi = 0;
        h = hash(h, (uint32_t)plat_ble_device() | (uint32_t)proj.ble_off << 4 | (uint32_t)btdev_pick() << 8);
        for (k = 0; k < 4 && plat_ble_found(k, name, &rssi); k++)
            h = hash(hash_s(h, name), (uint32_t)bt_bars(rssi));
    } else if (ui.view == V_SETUP && ui.setup_pg) {
        uint32_t k, top = 0;
        for (k = 0; k < NKEYS; k++)
            top = at_sent[k] > top ? at_sent[k] : top;
        h = hash(hash(h, 1u | ui.last_vel << 8 | top << 16), (uint32_t)touch_live | proj.touch << 1 | proj.at_mode << 2);
    } else if (ui.view == V_SETUP) {
        h = hash(hash(h, (uint32_t)proj.ble_off), plat_midi_count(PORT_BLE, 0) ^ plat_midi_count(PORT_BLE, 1) << 16);
        h = hash(hash(h, plat_midi_count(PORT_USB, 0) ^ plat_midi_count(PORT_USB, 1) << 16), plat_midi_count(PORT_TRS, 0));
    }
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    switch (ui.view) {
    case V_PLAY: draw_play(); break;
    case V_SCALE: draw_scale(); break;
    case V_ARP: draw_arp(); break;
    case V_SEQ: draw_seq(); break;
    case V_SOUND: draw_sound(); break;
    default: draw_setup(); break;
    }
    cv_blit(0, MAIN_Y);
}

/* the status line, on every page: the lowest key, the arp and sustain, and the program */
static void draw_status(void)
{
    char t[24];
    int32_t x;
    uint32_t h = hash(hash(hash(2166136261u, (uint32_t)(proj.octave + 3) | proj.scale << 4 | proj.root << 8 |
                                                 ui.sustain << 12 | ui.sus_lock << 16 | proj.arp_on << 13 | proj.arp_latch << 14 |
                                                 (uint32_t)(eng_mod_val() != 0) << 15),
                           proj.program | proj.sound << 8 | (uint32_t)proj.chan << 12), ui.view);
    if (h == ui.sig[3])
        return;
    ui.sig[3] = h;
    cv_begin(240, STAT_H, K_BG);
    put_note(put_s(t, "Lowest "), proj.scale ? SCALE0_NOTE + proj.root + 12 * proj.octave : KEY0_NOTE + 12 * proj.octave);
    x = cv_text(4, vc(&FONT_XS, 0, 15), &FONT_XS, t, K_DIM) + 8;
    if (proj.arp_on)
        x += tag(x, 0, proj.arp_latch ? "LATCH" : "ARP", K_HOT_T, K_HOT) + 4;
    if (ui.sustain)
        x += tag(x, 0, ui.sus_lock ? "SUS LOCK" : "SUSTAIN", K_HOT_T, K_HOT) + 4;
    if (eng_mod_val())
        tag(x, 0, "MOD", K_HOT_T, K_HOT);
    prog_name(t, proj.program);
    text_r(236, vc(&FONT_XS, 0, 15), &FONT_XS, t, K_TEXT);
    cv_blit(0, STAT_Y);
}

/* the knob row (X0X's): on a rule, a tag naming what the knobs edit, and dots for the page's sets (Play:
 * the four banks; Sequencer: the four tracks); four knobs, each an arc (a switch: a pip per position) with
 * its value; under them, the footer: what the page's buttons and keys do */
static void knob_tag(char *t, int *dots, int *cur)
{
    static const char *const T[NVIEWS] = {"BANK", "KEYS", "ARP", "TRACK", "SOUND", "SETUP"};
    char *b = put_s(t, T[ui.view]);
    *dots = 0;
    if (ui.view == V_PLAY) {
        b[0] = ' ';
        b[1] = (char)('A' + proj.bank);
        b[2] = 0;
        if (fx_cc())
            put_s(b + 2, ": CC NUMBERS");
        *dots = NBANKS;
        *cur = proj.bank;
    } else if (ui.view == V_SEQ) {
        put_u(put_s(b, " "), ui.track + 1u);
        *dots = NTRACKS;
        *cur = ui.track;
    } else if (ui.view == V_SETUP) {
        if (ui.setup_pg == 2)
            put_s(t, "BLUETOOTH");
        else if (ui.setup_pg)
            put_s(t, "TOUCH");
        *dots = SETUP_PAGES;
        *cur = ui.setup_pg;
    }
}
static void footer_text(const char **a, const char **b, const char **c)
{
    *a = *b = *c = 0;
    switch (ui.view) {
    case V_PLAY: *a = "SELECT, FX: BANK"; *b = "HOLD FX: SET CC"; *c = "ENV X2: SUS LOCK"; break;
    case V_SCALE:
        if (proj.scale) {
            *a = "WHITE: THE SCALE";
            *b = "BLACK: ITS CHORDS";
        } else {
            *a = "KNOB 2: A SCALE";
        }
        break;
    case V_ARP: *a = "HOLD NOTES OR A CHORD"; *b = "KNOB 1: OFF, ON, LATCH"; break;
    case V_SEQ: *a = "WHITE: STEPS"; *b = "SELECT, BLACK 1-4: TRACK"; *c = "5-8: MUTE"; break;
    case V_SOUND: *a = "PRESETS: THE SOUND"; *b = "MASTER: VOLUME"; break;
    default:
        if (ui.setup_pg == 2) {
            *a = "SELECT: SETUP";
            if (bt_stopped())
                *b = "SAVE: START BLUETOOTH";
            else if (FM1B_BLE_CENTRAL && !proj.ble_off && plat_ble_state() == BLE_READY) {
                *b = "KNOB 1: A DEVICE";
                *c = btdev_busy() ? "SAVE: DISCONNECT" : "SAVE: CONNECT";
            }
            break;
        }
        if (ui.setup_pg) {
            *a = "SELECT: SETUP";
            *b = "CURVE +: FIRMER";
            *c = "AT SENS +: LIGHTER";
            break;
        }
        *a = "SELECT: TOUCH";
        *b = bt_stopped() ? "HOLD SAVE: BLUETOOTH" : "SAVE: NOW";
        break;
    }
}
static int knob_switch(int k)                    /* a few named positions: pips, not an arc */
{
    return k == K_ARP || k == K_CLOCK || k == K_GLOW || k == K_SOUND || k == K_SNDIN || k == K_AOCT ||
           k == K_TOUCH || k == K_ATM || k == K_BT;
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | proj.bank << 16 | ui.track << 20 |
                                       (uint32_t)fx_cc() << 24 | (uint32_t)ui.setup_pg << 25), i;
    int now_touch = plat_ms() < ui.touch_until, dots, cur = 0;
    char t[32];
    for (i = 0; i < 4; i++)
        h = hash(hash(h, (uint32_t)knob_get(view_knob((int)i))), proj.cc[proj.bank][i]);
    h = hash(hash(h, (uint32_t)now_touch), (uint32_t)plat_ble_state() | proj.scale << 4);
    if (ui.view == V_SETUP && ui.setup_pg == 2)   /* the Device knob names what the scan finds */
        h = hash(h, (uint32_t)plat_ble_device() | (uint32_t)btdev_count() << 4 | (uint32_t)view_knob(0) << 8);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end the tag, then the dots */
        int32_t xd, xt;
        knob_tag(t, &dots, &cur);
        xd = 237 - dots * 5;
        xt = xd - (dots ? 5 : 0) - text_w(&FONT_XS, t);
        cv_rect(0, 5, xt - 5, 1, K_LINE);
        cv_text(xt, vc(&FONT_XS, 0, 11), &FONT_XS, t, K_HOT);
        for (i = 0; i < (uint32_t)dots; i++)
            cv_rect(xd + (int32_t)i * 5, 4, 3, 3, (int)i == cur ? K_HOT : K_LINE);
    }
    {   /* the footer */
        const char *a, *b, *c, *p[3];
        int32_t x = 4;
        footer_text(&a, &b, &c);
        p[0] = a;
        p[1] = b;
        p[2] = c;
        for (i = 0; i < 3 && p[i]; i++)
            x = cv_text(x, vc(&FONT_T, 71, 9), &FONT_T, p[i], K_DIM) + 7;
    }
    for (i = 0; i < 4; i++) {
        int k = view_knob((int)i), lo, hi, v;
        int32_t cx = (int32_t)i * 60 + 30;
        int hot = now_touch && ui.touched == (int)i;
        if (i)
            cv_rect((int32_t)i * 60, 14, 1, 50, K_LINE);
        if (k == K_NONE)
            continue;
        if (hot)
            cv_round((int32_t)i * 60 + 2, 11, 56, 56, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        knob_name(k, t);
        text_c(cx, vc(&FONT_XS, 13, 10), &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (knob_switch(k)) {                    /* its positions as pips */
            int n = hi - lo + 1, q, pw = 6, gap = 2, w0 = n * (pw + gap) - gap;
            for (q = 0; q < n; q++)
                cv_round(cx - w0 / 2 + q * (pw + gap), 33, pw, 9, 2, q == v - lo ? (hot ? K_HOT : K_COOL) : K_LINE);
        } else {
            arc(cx, 38, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, hot ? K_HOT : K_COOL);
        }
        knob_text(k, t);
        text_c(cx, vc(text_w(&FONT_B, t) > 56 ? &FONT_XS : &FONT_B, 53, 13), text_w(&FONT_B, t) > 56 ? &FONT_XS : &FONT_B, t,
               hot ? K_HOT : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

/* the lights: bright for what is on, dim (Glow) for the other function buttons so they can be read
 * in the dark; on the Sequencer page the keys show the pattern */
static void leds(void)
{
    uint32_t b = 1u << VIEW_BTN[ui.view], k = 0, db, dk = 0, i;
    if (ui.playing && eng_beat_on())             /* playing: PLAY green, bright on the beat */
        b |= LED_PLAY_GREEN;
    if (ui.sustain)
        b |= 1u << B_ENV;
    if (eng_mod_val())
        b |= 1u << B_LFO;
    if (proj.octave < 0)
        b |= 1u << B_OCTDN;
    if (proj.octave > 0)
        b |= 1u << B_OCTUP;
    /* PLAY is green, never its red (that would mean recording, which Ctl does not do): dim green when
     * stopped and between beats */
    db = (((1u << NB) - 1u) & ~b & ~(1u << B_PLAY)) | LED_PLAY_GREEN;
    if (ui.view == V_SEQ) {
        const track_t *t = trk();
        int pos = eng_track_pos(ui.track);
        for (i = 0; i < 16; i++) {
            uint32_t m = 1u << WHITE_K[i];
            int on = t->pat >> i & 1u;
            if (i >= t->steps)
                continue;
            if ((int)i == pos)
                on = !on;                /* the playhead: the step turned over */
            if (on)
                k |= m;
            else
                dk |= m;
        }
        for (i = 0; i < 4; i++) {
            if (i == ui.track)
                k |= 1u << BLACK_K[i];
            else
                dk |= 1u << BLACK_K[i];
            if (!proj.trk[i].mute)
                k |= 1u << BLACK_K[4 + i];
        }
    } else {
        for (i = 0; i < NKEYS; i++) {
            if ((ui.keys >> i & 1u) || key_heard((int)i))
                k |= 1u << i;
            else if (key_root((int)i))
                dk |= 1u << i;
        }
    }
    plat_lights(b, k, db, dk, proj.glow);
}

/* AUTOSAVE: a few seconds after a change, when nothing is held or touched (a flash erase stops
 * everything for a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || ui.btn || ui.keys || ui.playing || now - ui.act_t < AUTOSAVE_QUIET)
        return;
    if (project_save() == 0)
        ui.dirty = 0;
}

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns (a controller with its name: "CC 74 cutoff 64"); the channel and
 * the program; the Sequencer's steps and mutes; play and stop; what a button did (say). WHERE (the reader asks, or has
 * just started listening) is everything on the screen. The pressure bar is never said (it changes all the
 * time), nor the MIDI counters but on WHERE. Nothing is built while no reader listens. The screen's short
 * words are said in full by the words file (firmware/words.tsv); the rest is said here in words. */
static char sp_buf[ANN_MAX + 1] SP_POOL;
static uint32_t sp_n;
static const char *const SCALE_SAID[NSCALES] = {"chromatic", "major", "minor", "dorian", "mixolydian",
                                                "harmonic minor", "pentatonic", "pentatonic minor", "blues"};
static const char *const CLOCK_SAID[NCLK] = {"internal and sent out", "internal only", "external"};
static const char *const AT_SAID[NAT] = {"off", "per note", "for the channel"};
static const char *const DRUM_SAID[21] = {"acoustic kick", "kick", "rimshot", "snare", "clap", "electric snare",
                                          "low floor tom", "closed hat", "high floor tom", "pedal hat", "low tom",
                                          "open hat", "low mid tom", "high mid tom", "crash", "high tom", "ride",
                                          "tambourine", "cowbell", "maracas", "claves"};
/* the controllers with a name of their own (MIDI 1.0) */
static const uint8_t CCN_N[25] = {0, 1, 2, 4, 5, 6, 7, 8, 10, 11, 64, 65, 66, 67, 68, 71, 72, 73, 74, 75, 76, 77,
                                  78, 91, 93};
static const char *const CCN_S[25] = {"bank select", "mod wheel", "breath", "foot pedal", "portamento time",
                                      "data entry", "volume", "balance", "pan", "expression", "sustain",
                                      "portamento", "sostenuto", "soft pedal", "legato", "resonance", "release",
                                      "attack", "cutoff", "decay", "vibrato rate", "vibrato depth", "vibrato delay",
                                      "reverb", "chorus"};

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_num(uint32_t v)
{
    char t[12];
    put_u(t, v);
    sp_cat(t);
}
static void sp_note(int n)                       /* "C4", "C sharp minus 1" (the screen's "C#-1") */
{
    char t[8];
    if (n < 12) {
        sp_cat(NOTE_NAME[n % 12]);
        sp_cat(" minus 1");
        return;
    }
    put_note(t, n);
    sp_cat(t);
}
static int sp_cc(uint32_t n)                     /* "CC 74 cutoff"; 1 if it has a name */
{
    uint32_t i;
    sp_cat("CC ");
    sp_num(n);
    for (i = 0; i < sizeof CCN_N; i++)
        if (CCN_N[i] == n) {
            sp_cat(" ");
            sp_cat(CCN_S[i]);
            return 1;
        }
    return 0;
}
static void sp_knob(int k)                       /* a knob's readout: its name, then its value */
{
    char t[24];
    int v = knob_get(k), d;
    if (k <= K_VAL3) {
        sp_cat(sp_cc(proj.cc[proj.bank][k - K_VAL0]) ? " " : " value ");
        sp_num((uint32_t)v);
        return;
    }
    if (k == K_MASTER) {
        sp_cat("Master ");
        if (v)
            sp_cc((uint32_t)v);
        else
            sp_cat("volume");
        return;
    }
    knob_name(k, t);
    sp_cat(t);
    sp_cat(k <= K_CC3 ? ", " : " ");
    switch (k) {
    case K_CC0: case K_CC1: case K_CC2: case K_CC3: sp_cc((uint32_t)v); return;
    case K_SCALE: sp_cat(SCALE_SAID[v]); return;
    case K_CLOCK: sp_cat(CLOCK_SAID[v]); return;
    case K_TOUCH: sp_cat(v ? "from the keys" : "fixed"); return;
    case K_ATM: sp_cat(AT_SAID[v]); return;
    case K_TEMPO:
        if (proj.clock == CLK_EXT) {
            sp_cat("external clock");
            return;
        }
        break;
    case K_NOTE:
        if ((d = drum_index(v)) >= 0) {
            sp_cat(DRUM_SAID[d]);
            return;
        }
        break;
    default: break;
    }
    knob_text(k, t);
    sp_cat(t);
}

static void sp_program(void)                     /* "Program 53, Choir Aahs" */
{
    sp_n = 0;
    sp_cat("Program ");
    sp_num(proj.program + 1u);
    if (proj.sound != SOUND_OFF) {
        sp_cat(", ");
        sp_cat(proj.chan == 9 ? "drum kit" : GM_PROGRAM[proj.program]);
    }
}
static void sp_channel(void)
{
    sp_n = 0;
    sp_cat("Channel ");
    sp_num(proj.chan + 1u);
    if (proj.chan == 9 && proj.sound != SOUND_OFF)
        sp_cat(", drum kit");
}

/* the page's name: "Play page, bank A", "Sequencer page, track 2", "Touch page" */
static void sp_page(void)
{
    char t[2] = {(char)('A' + proj.bank), 0};
    if (fx_cc()) {
        sp_cat("Controller numbers, bank ");
        sp_cat(t);
        return;
    }
    if (ui.view == V_SETUP && ui.setup_pg == 2) {
        int d = plat_ble_device();
        sp_cat("Bluetooth page");
        if (proj.ble_off || plat_ble_state() != BLE_READY) {
            sp_cat(", Bluetooth ");
            sp_cat(bt_stopped() ? "stopped after a crash. SAVE starts it" : bt_status());
            return;
        }
        sp_cat(plat_ble_connected() ? ", a computer connected" : plat_ble_linked() ?
               ", a computer connected, its MIDI not started. On a Mac, reopen Audio MIDI Setup" : ", no computer connected");
        if (d == BLE_DEV_READY) {
            sp_cat(", connected to ");
            sp_cat(btdev_name());
        } else if (d == BLE_DEV_CONNECTING || d == BLE_DEV_SEARCHING) {
            sp_cat(", connecting to ");
            sp_cat(btdev_name());
        } else if (FM1B_BLE_CENTRAL) {
            sp_cat(", ");
            sp_num((uint32_t)btdev_count());
            sp_cat(btdev_count() == 1 ? " device found" : " devices found");
        }
        return;
    }
    if (ui.view == V_SETUP && ui.setup_pg) {
        sp_cat("Touch page");
        return;
    }
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    if (ui.view == V_PLAY) {
        sp_cat(", bank ");
        sp_cat(t);
    } else if (ui.view == V_SEQ) {
        sp_cat(", track ");
        sp_num(ui.track + 1u);
    }
}

/* the page and its knobs; all: what the page shows, the top bar and the status line too */
static void sp_screen(int all)
{
    int i, n = 0;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_page();
    for (i = 0; i < 4; i++) {
        int k = view_knob(i);
        if (k == K_NONE)
            continue;
        sp_cat(n++ ? ", " : ": ");
        sp_knob(k);
    }
    if (!all)
        return;
    switch (ui.view) {
    case V_SCALE:
        if (!proj.scale) {
            sp_cat(". The keys play every note");
            break;
        }
        sp_cat(". Notes ");                      /* the white keys' (the black keys: the chords on them) */
        for (i = 0; i < SCALE_LEN[proj.scale]; i++) {
            const char *nm = NOTE_NAME[(proj.root + SCALE_IV[proj.scale][i]) % 12];
            if (i)                               /* "D and D sharp": a note and its sharp, together */
                sp_cat(nm[0] == NOTE_NAME[(proj.root + SCALE_IV[proj.scale][i - 1]) % 12][0] ? " and " : ", ");
            sp_cat(nm);
        }
        break;
    case V_SEQ: {
        const track_t *t = trk();
        sp_cat(". Steps on: ");
        for (i = 0, n = 0; i < t->steps; i++)
            if (t->pat >> i & 1u) {
                if (n++)
                    sp_cat(", ");
                sp_num((uint32_t)i + 1u);
            }
        if (!n)
            sp_cat("none");
        for (i = 0; i < NTRACKS; i++)
            if (proj.trk[i].mute) {
                sp_cat(". Track ");
                sp_num((uint32_t)i + 1u);
                sp_cat(" muted");
            }
        break;
    }
    case V_SOUND:
        if (proj.sound == SOUND_OFF) {
            sp_cat(". The onboard sounds are off");
            break;
        }
        sp_cat(". Sound ");
        if (proj.chan == 9) {
            sp_cat("drum kit");
        } else {
            sp_cat(GM_PROGRAM[gm_program[proj.chan]]);
            sp_cat(", program ");
            sp_num(gm_program[proj.chan] + 1u);
        }
        sp_cat(proj.in_mute ? ". It plays the keys, not MIDI in" : ". It plays the keys and MIDI in");
        break;
    case V_SETUP:
        if (ui.setup_pg == 2)
            break;
        if (ui.setup_pg) {
            if (!touch_live)
                sp_cat(". Every note plays at the velocity set on the Keys page");
            else if (ui.last_vel) {
                sp_cat(". Last velocity ");
                sp_num(ui.last_vel);
            }
            break;
        }
        if (FM1B_BLE) {
            int s = plat_ble_state();
            sp_cat(". Bluetooth ");
            sp_cat(bt_stopped() ? "stopped after a crash" : bt_status());
            if (s != BLE_OFF && s != BLE_STARTING) {
                sp_cat(", in ");
                sp_num(plat_midi_count(PORT_BLE, 0));
                sp_cat(", out ");
                sp_num(plat_midi_count(PORT_BLE, 1));
            }
        }
        sp_cat(". USB in ");
        sp_num(plat_midi_count(PORT_USB, 0));
        sp_cat(", out ");
        sp_num(plat_midi_count(PORT_USB, 1));
        sp_cat(". TRS in ");
        sp_num(plat_midi_count(PORT_TRS, 0));
        sp_cat(". CTL-1 " APP_VERSION);
        break;
    default: break;
    }
    sp_cat(". Channel ");                        /* the top bar */
    sp_num(proj.chan + 1u);
    if (proj.clock == CLK_EXT) {
        sp_cat(", external clock");
    } else {
        sp_cat(", tempo ");
        sp_num(proj.tempo);
    }
    sp_cat(ui.playing ? ", playing" : ", stopped");
    if (ui.view == V_SEQ || ui.view == V_SETUP)       /* the status line where it matters (and fits) */
        return;
    if (ui.view != V_SOUND) {
        sp_cat(". Lowest key ");
        sp_note(proj.scale ? SCALE0_NOTE + proj.root + 12 * proj.octave : KEY0_NOTE + 12 * proj.octave);
    }
    if (ui.view == V_PLAY || ui.view == V_ARP) {
        sp_cat(". Program ");
        sp_num(proj.program + 1u);
        if (proj.sound != SOUND_OFF) {
            sp_cat(", ");
            sp_cat(proj.chan == 9 ? "drum kit" : GM_PROGRAM[proj.program]);
        }
    }
    if (proj.arp_on && ui.view != V_ARP)
        sp_cat(proj.arp_latch ? ". Arp latched" : ". Arp on");
    if (ui.sustain)
        sp_cat(ui.sus_lock ? ". Sustain locked" : ". Sustain on");
}

static uint32_t sp_ctx_sig(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)fx_cc() << 8);
    if (ui.view == V_PLAY)
        h = hash(h, proj.bank);
    else if (ui.view == V_SEQ)
        h = hash(h, ui.track);
    else if (ui.view == V_SETUP)
        h = hash(h, ui.setup_pg);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)(view_knob(i) == K_NONE ? 0 : knob_get(view_knob(i)));
    sp.chan = proj.chan;
    sp.program = proj.program;
    sp.playing = ui.playing;
    sp.sus_lock = ui.sus_lock;
    sp.ble = (uint8_t)plat_ble_connected();
    sp.touch_until = ui.touch_until;
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1, ev = sp.ev;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        sp.ev = SE_NONE;
        return;
    }
    sp.ev = SE_NONE;
    if (!sp.started || where) {                       /* everything on the screen */
        sp.started = 1;
        sp.ctx = sp_ctx_sig();
        sp_snap();
        sp_screen(1);
        ann_say(sp_buf, 0, ANN_NOW);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (ev == SE_PANIC)
        sp.sus_lock = ui.sus_lock;                    /* (all notes off says it: sustain off too) */
    if (ui.playing != sp.playing) {
        sp.playing = ui.playing;
        ann_say(ui.playing ? "Play" : "Stop", 0, proj.clock == CLK_EXT ? 0u : ANN_NOW);   /* from outside: queued */
    }
    if (FM1B_BLE && plat_ble_connected() != sp.ble) {   /* not the user's doing: queued */
        sp.ble = (uint8_t)plat_ble_connected();
        ann_say(sp.ble ? "Computer connected" : "Computer disconnected", 0, 0);
    }
    if (ui.sus_lock != sp.sus_lock) {                 /* locking says SUSTAIN LOCKED itself */
        sp.sus_lock = ui.sus_lock;
        if (!ui.sus_lock)
            ann_say("Sustain unlocked", 0, ANN_NOW);
    }
    c = sp_ctx_sig();
    if (c != sp.ctx) {                                /* a new page: said when it settles */
        sp.ctx = c;
        sp_snap();
        sp_screen(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    if (proj.chan != sp.chan) {                       /* ALGORITHM */
        sp_snap();
        sp_channel();
        ann_later(sp_buf, ANN_NOW, 120u, now);
        return;
    }
    if (proj.program != sp.program) {                 /* PRESETS */
        sp_snap();
        sp_program();
        ann_later(sp_buf, ANN_NOW, 120u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int k = view_knob(i), v = k == K_NONE ? 0 : knob_get(k);
        if (v != sp.val[i] && changed < 0)
            changed = i;
    }
    if (changed < 0 && ui.touch_until != sp.touch_until && ui.touched >= 0 && view_knob(ui.touched) != K_NONE)
        changed = ui.touched;                         /* turned, but at its end (or Tempo on an outside clock) */
    if (changed >= 0) {
        sp_snap();
        sp_n = 0;
        sp_knob(view_knob(changed));
        ann_later(sp_buf, ANN_NOW, 120u, now);
        return;
    }
    switch (ev) {                                     /* the keys */
    case SE_STEP:
        sp_n = 0;
        sp_cat("Step ");
        sp_num(sp.ev_arg + 1u);
        sp_cat(sp.ev_on ? " on" : " off");
        ann_say(sp_buf, 0, ANN_NOW);
        break;
    case SE_STEP_OUT:
        sp_n = 0;
        sp_cat("Step ");
        sp_num(sp.ev_arg + 1u);
        sp_cat(" not set: track ");
        sp_num(ui.track + 1u);
        sp_cat(" has ");
        sp_num(trk()->steps);
        sp_cat(" steps");
        ann_say(sp_buf, 0, ANN_NOW | ANN_KEEP);
        break;
    case SE_MUTE:
        sp_n = 0;
        sp_cat("Track ");
        sp_num(sp.ev_arg + 1u);
        sp_cat(sp.ev_on ? " muted" : " unmuted");
        ann_say(sp_buf, 0, ANN_NOW);
        break;
    case SE_PANIC: ann_say("All notes off", 0, ANN_NOW); break;
    default: break;
    }
}

#ifdef PLAT_HOST
/* every page, knob value, chord and message as a screen reader gets them (the "speakdump" command):
 * KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void sp_dump_page(void)
{
    project_t keep = proj;
    int i, x, lo, hi;
    sp_screen(0);
    sp_print("SCREEN", sp_buf);
    sp_screen(1);
    sp_print("WHERE", sp_buf);
    for (i = 0; i < 4; i++) {
        int k = view_knob(i);
        if (k == K_NONE)
            continue;
        knob_range(k, &lo, &hi);
        for (x = lo; x <= hi; x++) {
            knob_set(k, x);
            sp_n = 0;
            sp_knob(k);
            sp_print("KNOB", sp_buf);
        }
        proj = keep;
        touch_apply();
    }
}
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"LOWEST KEY", "F#3"}, {"LOWEST KEY", "C0"}, {"SUSTAIN", "LOCKED"},
                                         {"SAVED", 0}, {"BLUETOOTH", "STARTING"}, {"BLUETOOTH OFF", "AFTER A CRASH"},
                                         {"BLUETOOTH", "ON"}, {"BLUETOOTH", "OFF"}, {"UPDATE MODE IN", "3.."},
                                         {"UPDATE MODE", "CANCELLED"}};
    static uint8_t keep_ui[sizeof ui];
    project_t keep = proj;
    int v, q, x, i, k;
    memcpy(keep_ui, &ui, sizeof ui);
    for (v = 0; v < NVIEWS; v++) {                    /* every page (Play: each bank, and FX held; Sequencer:
                                                       * each track, on channel 10 and on another) */
        ui.view = (uint8_t)v;
        for (q = 0; q < (v == V_PLAY ? 2 * NBANKS : v == V_SEQ ? 2 * NTRACKS : v == V_SETUP ? SETUP_PAGES : 1); q++) {
            ui.btn = 0;
            ui.setup_pg = (uint8_t)(v == V_SETUP ? q : 0);
            if (v == V_PLAY) {
                proj.bank = (uint8_t)(q % NBANKS);
                ui.btn = q >= NBANKS ? 1u << B_FX : 0u;
                ui.fx_used = 1;
            } else if (v == V_SEQ) {
                ui.track = (uint8_t)(q % NTRACKS);
                proj.seq_ch = (uint8_t)(q < NTRACKS ? 9 : 0);
            }
            sp_dump_page();
            proj = keep;
        }
    }
    ui.btn = 0;
    ui.view = V_PLAY;                                 /* WHERE in other states */
    for (x = 0; x < 8; x++) {
        proj.sound = (uint8_t)(x & 1 ? SOUND_OFF : SOUND_IN);
        proj.chan = (uint8_t)(x & 2 ? 9 : 0);
        proj.clock = (uint8_t)(x & 4 ? CLK_EXT : CLK_INT_OUT);
        proj.arp_on = (uint8_t)(x & 1);
        proj.arp_latch = (uint8_t)(x >> 1 & 1);
        ui.playing = (uint8_t)(x >> 2 & 1);
        ui.sustain = (uint8_t)(x >> 1 & 1);
        ui.sus_lock = (uint8_t)(x & 1);
        sp_screen(1);
        sp_print("WHERE", sp_buf);
        sp_n = 0;
        sp_knob(K_TEMPO);
        sp_print("KNOB", sp_buf);
    }
    proj = keep;
    ui.playing = ui.sustain = ui.sus_lock = 0;
    ui.view = V_SCALE;
    for (x = 0; x < NSCALES; x++)
        for (i = 0; i < 12; i++) {
            proj.scale = (uint8_t)x;
            proj.root = (uint8_t)i;
            sp_screen(1);
            sp_print("WHERE", sp_buf);
        }
    proj = keep;
    ui.view = V_SEQ;                                  /* the steps: none, all, muted */
    for (x = 0; x < 3; x++) {
        proj.trk[0].pat = x == 1 ? 0xFFFFu : 0u;
        proj.trk[1].mute = proj.trk[3].mute = (uint8_t)(x == 2);
        sp_screen(1);
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.view = V_SETUP;                                /* the Touch page: off, on with a velocity */
    ui.setup_pg = 1;
    for (x = 0; x < 2; x++) {
        proj.touch = (uint8_t)x;
        touch_apply();
        ui.last_vel = 87;
        sp_screen(1);
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    touch_apply();
    for (x = 0; x < 128; x++) {                       /* PRESETS, ALGORITHM */
        proj.program = (uint8_t)x;
        proj.sound = SOUND_IN;
        sp_program();
        sp_print("VALUE", sp_buf);
    }
    proj.sound = SOUND_OFF;
    sp_program();
    sp_print("VALUE", sp_buf);
    proj.sound = SOUND_IN;
    for (x = 0; x < 16; x++) {
        proj.chan = (uint8_t)x;
        sp_channel();
        sp_print("VALUE", sp_buf);
        proj.chan = 9;
        sp_program();
        sp_print("VALUE", sp_buf);
    }
    proj = keep;
    ui.view = V_SOUND;
    k = gm_program[0];
    for (x = 0; x < 16; x++) {
        gm_program[0] = (uint8_t)(x * 8 + 3);
        proj.in_mute = (uint8_t)(x & 1);
        sp_screen(1);
        sp_print("WHERE", sp_buf);
    }
    gm_program[0] = (uint8_t)k;
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], sp_buf);
        sp_print("MSG", sp_buf);
    }
    msg_text("NOT SAVED", "FLASH ERROR", sp_buf);
    sp_print("WARN", sp_buf);
    sp_print("WARN", "Step 13 not set: track 4 has 12 steps");
    {
        static const char *const SAID[] = {"Play", "Stop", "Computer connected", "Computer disconnected",
                                           "Sustain unlocked", "All notes off", "Step 5 on", "Step 5 off",
                                           "Track 2 muted", "Track 2 unmuted"};
        for (i = 0; i < (int)(sizeof SAID / sizeof SAID[0]); i++)
            sp_print("MSG", SAID[i]);
    }
    proj = keep;
    memcpy(&ui, keep_ui, sizeof ui);
    touch_apply();
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.master_sent = -1;
    ui.master_raw = (int32_t)plat_master();
    lcd_fill(0, 0, 240, 240, K_BG);
    touch_on(0);
    touch_apply();                       /* Touch as saved */
    synth_program();                     /* the saved program, which the screen shows */
}

static void dev_scan(void);
void ui_frame(void)
{
    input();
    dev_scan();
    autosave();
    draw_header();
    plat_yield();
    draw_main();
    plat_yield();
    draw_status();
    draw_knobs();
    leds();
    speak();                             /* a screen reader: what changed (announce.c) */
    ann_tick(plat_ms());
    ui.frame++;
}

/* the Bluetooth page scans for devices while it is up (fm1-base's lib/btdev); a device connected, given up or lost
 * is said */
static void dev_scan(void)
{
    int ev;
    btdev_page(ui.view == V_SETUP && ui.setup_pg == 2 && !proj.ble_off);
    while ((ev = btdev_event()) != BTDEV_NONE)
        if (ev == BTDEV_CONNECTED || ev == BTDEV_NO_ANSWER || ev == BTDEV_LOST)
            say(btdev_word(ev), btdev_name());
        else if ((ev == BTDEV_COMPUTER || ev == BTDEV_COMPUTER_GONE) && !proj.ble_off)   /* (not switching off) */
            say_as("COMPUTER", ev == BTDEV_COMPUTER ? "CONNECTED" : "DISCONNECTED", 0);
}
void ui_input_only(void)
{
    input();
    dev_scan();
}
