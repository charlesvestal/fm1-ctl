/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl UI: a MIDI controller. Part of the unity build (after gfx.c, project.c and engine.c); the host
 * simulator includes it too.
 *
 *   keys          chromatic from F3; with a scale: white keys the scale's steps, black keys its chords
 *   KNOB 1-4      the page's four values (on Play: the knob bank's four controllers)
 *   SELECT tempo  ALGORITHM MIDI channel  PRESETS program change
 *   FX            the next knob bank (A..D); held on Play, KNOB 1-4 set the bank's controller numbers
 *   ARP           the Arp page (KNOB 1 there turns it on: Off / On / Latch)
 *   PLAY          start / stop (the sequencer, MIDI start / stop)    REC  panic (all notes off)
 *   ENV           sustain while held            OCT- / OCT+  the octave (-3..+3)
 *   LFO           the mod wheel (CC 1): up over a sixteenth while held, back down after
 *   pages         one button each: HOME Play, SEL Keys, ARP Arp, SEQ Sequencer, GLO Sound, EDIT Setup
 *   SAVE          saves (it also saves by itself, a few seconds after a change)
 *
 * On the Sequencer page the keys edit instead of playing: white keys the selected track's 16 steps,
 * black keys 1-4 select a track, 5-8 mute one.
 *
 * Out: everything played goes to USB and Bluetooth. Thru: USB in -> Bluetooth, Bluetooth in -> USB,
 * the TRS jack in -> USB and Bluetooth. */

enum { V_PLAY, V_SCALE, V_ARP, V_SEQ, V_SOUND, V_SETUP, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Play", "Keys", "Arp", "Sequencer", "Sound", "Setup"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_SEL, B_ARP, B_SEQ, B_GLO, B_EDIT};
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
#include "gm_names.h"
#define KEY0_NOTE 53                   /* chromatic: F3 is key 0 at octave 0 */
#define SCALE0_NOTE 48                 /* a scale: its root from C3 at octave 0 */
#define FLASH_MS 120u                  /* a port's light after a message */

/* the FM-1's keys: 16 white, 11 black, as the keyboard runs F3..G5 */
static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t BLACK_K[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};

/* the screen's colours (FoMni's): cream paper, warm brown ink, coral for what is played, teal for
 * controllers and connections */
#define K_BG RGB(250, 244, 232)
#define K_PANEL RGB(236, 228, 212)
#define K_LINE RGB(216, 205, 186)
#define K_DIM RGB(150, 136, 118)
#define K_TEXT RGB(72, 58, 48)
#define K_WHITE RGB(255, 255, 255)
#define K_HOT RGB(242, 120, 72)
#define K_HOT_T RGB(248, 208, 184)
#define K_COOL RGB(38, 166, 154)
#define K_COOL_T RGB(196, 230, 222)
#define K_KEYW RGB(255, 252, 245)
#define K_KEYB RGB(110, 94, 80)

enum { KM_NONE, KM_NOTES, KM_ARP };    /* what a held key did: sent notes, gave them to the arpeggiator */
static struct {
    uint8_t view;
    uint8_t track;                     /* the sequencer track the Sequencer page edits */
    uint32_t btn, keys;
    uint32_t enc_t[NE];
    uint8_t kmode[NKEYS], knotes[NKEYS][3], kn[NKEYS];   /* per held key: how, which notes */
    uint32_t heard[4];                 /* notes held from MIDI in (any port), for the key lights */
    uint8_t playing;
    uint8_t sustain;
    uint8_t fx_used;                   /* a knob turned while FX was held: no bank change at its release */
    uint32_t fx_t;                     /* when FX went down */
    uint8_t dirty;
    uint32_t act_t;
    uint32_t in_t[NPORTS], out_t[NPORTS];   /* when each port last carried a message */
    uint32_t n_in[NPORTS], n_out[NPORTS];
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

static void say(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
}
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

static void send(uint32_t pkt, uint32_t to)
{
    uint32_t p, now = plat_ms();
    plat_midi_send(pkt, to);
    for (p = 0; p < 2u; p++)
        if (to >> p & 1u) {
            ui.out_t[p] = now;
            ui.n_out[p]++;
        }
}
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

static void key_down(int k)
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
            play(0x90u, q[i], proj.vel);
    }
    b = put_note(put_s(ui.last, n > 1 ? "Chord " : "Note "), q[0]);
    if (!proj.arp_on)
        put_u(put_s(b, "  "), proj.vel);
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
    ui.sustain = 0;
    put_s(ui.last, "All notes off");
}

/* MIDI in: forwarded (USB -> BLE, BLE -> USB, TRS -> both), noted for the lights; clock and
 * start / stop drive the engine when the clock is external */
static const uint32_t THRU[NPORTS] = {TO_BLE, TO_USB, TO_USB | TO_BLE};
static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (port < 0 || port >= NPORTS)
            continue;
        ui.in_t[port] = plat_ms();
        ui.n_in[port]++;
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
    }
}

/* ------------------------------------------------------------ values --- */
/* what a knob edits: one of these */
enum { K_VAL0, K_VAL1, K_VAL2, K_VAL3, K_CC0, K_CC1, K_CC2, K_CC3, K_ROOT, K_SCALE, K_VEL, K_HUMAN,
       K_AMODE, K_ARATE, K_AOCT, K_ARP, K_STEPS, K_HITS, K_ROT, K_NOTE, K_CLOCK, K_SEQCH, K_MASTER, K_GLOW,
       K_BT, K_SOUND, K_SNDIN, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {K_VAL0, K_VAL1, K_VAL2, K_VAL3},
    {K_ROOT, K_SCALE, K_VEL, K_HUMAN},
    {K_ARP, K_AMODE, K_ARATE, K_AOCT},
    {K_STEPS, K_HITS, K_ROT, K_NOTE},
    {K_SOUND, K_SNDIN, K_MASTER, K_NONE},
    {K_CLOCK, K_SEQCH, K_GLOW, K_BT},
};
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
    case K_BT: return plat_ble_state() != BLE_OFF;
    case K_SOUND: return proj.sound != SOUND_OFF;
    case K_SNDIN: return !proj.in_mute;
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
    case K_BT: case K_SOUND: case K_SNDIN: *hi = 1; break;
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
                                    "Bluetooth", "Sound", "MIDI in"};
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
    case K_BT: {
        static const char *const S[] = {"Off", "On", "On", "Failed"};
        put_s(b, S[plat_ble_state() & 3]);
        break;
    }
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
        case K_BT:
            if (v && plat_ble_state() == BLE_OFF) {
                plat_ble_start();
                say("BLUETOOTH", "STARTING");
            } else if (!v) {
                say("BLUETOOTH", "IS ALWAYS ON");
            }
            return;                      /* nothing saved: Bluetooth starts at every boot */
        default: return;
        }
    }
    mark_dirty();
}

/* knob acceleration by speed: a slow turn is one step a detent, a quick one up to 6 */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
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

static void button_down(int b)
{
    switch (b) {
    case B_ENV:
        ui.sustain = 1;
        cc(64, 127);
        break;
    case B_LFO: engine_mod(1); break;
    case B_PLAY:
        ui.playing = (uint8_t)!ui.playing;
        engine_play(ui.playing);
        put_s(ui.last, ui.playing ? "Start" : "Stop");
        break;
    case B_REC: panic(); break;
    case B_OCTDN: octave(-1); break;
    case B_OCTUP: octave(1); break;
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
        ui.sustain = 0;
        cc(64, 0);
        break;
    case B_LFO: engine_mod(0); break;
    case B_FX:
        if (!ui.fx_used) {
            char t[2] = {0, 0};
            proj.bank = (uint8_t)((proj.bank + 1u) % NBANKS);
            t[0] = (char)('A' + proj.bank);
            say("KNOB BANK", t);
            mark_dirty();
        }
        ui.touched = -1;
        break;
    case B_HOME: set_view(V_PLAY); break;
    case B_SEL: set_view(V_SCALE); break;
    case B_SEQ: set_view(V_SEQ); break;
    case B_ARP: set_view(V_ARP); break;
    case B_GLO: set_view(V_SOUND); break;
    case B_EDIT: set_view(V_SETUP); break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            say("NOT SAVED", "FLASH ERROR");
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
        }
    } else if (b >= 0 && b < 4) {
        ui.track = (uint8_t)b;
        ui.touched = -1;
    } else if (b >= 4 && b < 8) {
        proj.trk[b - 4].mute = (uint8_t)!proj.trk[b - 4].mute;
        mark_dirty();
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
            if (keys >> i & 1u) {
                if (ui.view == V_SEQ)
                    seq_key((int)i);
                else
                    key_down((int)i);
            } else {
                key_up((int)i);
            }
        }
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
    if ((e = plat_enc(EN_SELECT)) != 0 && proj.clock != CLK_EXT) {
        proj.tempo = (uint8_t)clamp(proj.tempo + accel(EN_SELECT, e, 200), 40, 240);
        mark_dirty();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        int c = clamp(proj.chan + e, 0, 15);
        if (c != proj.chan) {
            all_keys_up();               /* held notes end on the channel they started on */
            engine_panic();
            proj.chan = (uint8_t)c;
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
/* four bands: the header (28 px), the page (124), the status line (20), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 124
#define STAT_Y 152
#define STAT_H 20
#define KNB_Y 172
#define KNB_H 68

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

static int32_t tag(int32_t x, int32_t y, const char *s, uint16_t bg, uint16_t fg)   /* a little label */
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(x, y, w, 16, 6, bg);
    cv_text(x + 5, y + 1, &FONT_XS, s, fg);
    return w;
}

static int lit(const uint32_t *t, int p) { return plat_ms() - t[p] < FLASH_MS && t[p]; }

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until, bs = plat_ble_state(), bc = plat_ble_connected(), beat = eng_beat_on();
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
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_HOT);
    } else {
        int32_t x = cv_text(10, 6, &FONT_B, VIEW_NAME[ui.view], K_TEXT) + 6;
        if (ui.view == V_PLAY) {
            char b[2] = {(char)('A' + proj.bank), 0};
            x += tag(x, 6, b, K_COOL_T, K_COOL) + 4;
        }
        if (ui.dirty)
            cv_round(x + 2, 12, 5, 5, 2, K_DIM);
        x = 232;                         /* right: channel, tempo with the beat, Bluetooth */
        put_u(put_s(t, "CH"), proj.chan + 1u);
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, K_TEXT);
        if (proj.clock == CLK_EXT)
            put_s(t, "EXT");
        else
            put_u(t, proj.tempo);
        x -= text_w(&FONT_S, t) + 8;
        cv_text(x, 6, &FONT_S, t, ui.playing ? K_TEXT : K_DIM);
        x -= 12;
        cv_round(x, 10, 8, 8, 4, beat ? (ui.playing ? K_HOT : K_LINE) : K_PANEL);
        if (bs != BLE_OFF) {
            uint16_t c = bc ? K_COOL : bs == BLE_FAILED ? K_HOT : bs == BLE_READY ? K_COOL_T :
                         (ui.frame / 8u) & 1u ? K_COOL : K_COOL_T;
            int32_t w = text_w(&FONT_XS, "BLE") + 10;
            x -= w + 6;
            cv_round(x, 6, w, 16, 6, c);
            cv_text(x + 5, 7, &FONT_XS, "BLE", bc ? K_WHITE : K_TEXT);
        }
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
    int p;
    for (p = 0; p < NPORTS; p++) {
        int32_t x = 8 + p * 78;
        int i = lit(ui.in_t, p), o = p < 2 && lit(ui.out_t, p);
        const char *st = "";
        uint16_t sc = K_DIM;
        cv_round(x, y0, 70, 34, 6, K_PANEL);
        cv_text(x + 8, y0 + 2, &FONT_S, PORT_NAME[p], K_TEXT);
        cv_round(x + 52, y0 + 5, 8, 8, 4, i ? K_COOL : K_LINE);   /* in */
        if (p < 2)
            cv_round(x + 52, y0 + 18, 8, 8, 4, o ? K_HOT : K_LINE);   /* out */
        if (p == PORT_BLE) {
            int s = plat_ble_state();
            st = plat_ble_connected() ? "connected" : s == BLE_READY ? "waiting" : s == BLE_STARTING ? "starting" :
                 s == BLE_FAILED ? "failed" : "off";
            sc = plat_ble_connected() ? K_COOL : K_DIM;
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
    cv_text(12, 22, &FONT_S, t, K_COOL);
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
static void track_name(int n, char *b)
{
    static const uint8_t N[] = {35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 54, 56, 70, 75};
    static const char *const S[] = {"Kick", "Kick", "Rim", "Snare", "Clap", "Snare", "Tom", "Hat", "Tom", "Pedal",
                                    "Tom", "Open", "Tom", "Tom", "Crash", "Tom", "Ride", "Tamb", "Bell", "Shake",
                                    "Clave"};
    uint32_t i;
    if (proj.seq_ch == 9)
        for (i = 0; i < sizeof N; i++)
            if (N[i] == n) {
                put_s(b, S[i]);
                return;
            }
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

static void draw_setup(void)                     /* the settings below; Bluetooth and its log here */
{
    char t[40];
    int s = plat_ble_state(), i, n;
    const char *st = plat_ble_connected() ? "connected" : s == BLE_READY ? "advertising" :
                     s == BLE_STARTING ? "starting" : s == BLE_FAILED ? "failed" : "off";
    put_s(put_s(t, "Bluetooth "), st);
    cv_text(10, 2, &FONT_S, t, plat_ble_connected() ? K_COOL : K_TEXT);
    cv_text(236 - text_w(&FONT_XS, "Ctl " CT_VERSION), 4, &FONT_XS, "Ctl " CT_VERSION, K_LINE);
    if (s == BLE_OFF) {
        cv_text(10, 20, &FONT_XS, "Off after a crash: KNOB 4 starts it", K_DIM);
    } else {
        char *b = put_u(put_s(t, "Name FM-1   in "), ui.n_in[PORT_BLE]);
        put_u(put_s(b, "   out "), ui.n_out[PORT_BLE]);
        cv_text(10, 20, &FONT_XS, t, K_DIM);
    }
    n = (int)plat_ble_log_total();       /* the log, the newest last (kept through a reset): seven lines, */
    if (n > 7)                           /* the whole crash note */
        n = 7;
    for (i = 0; i < n; i++)
        cv_text(8, 36 + (6 - (n - 1 - i)) * 12, &FONT_XS, plat_ble_log_line(n - 1 - i), K_DIM);
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
    cv_text(12, 46, &FONT_XS, proj.in_mute ? "Plays Ctl's notes (not MIDI in)" : "Plays Ctl's notes and MIDI in", K_DIM);
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
        h = hash(h, (uint32_t)lit(ui.in_t, (int)p) | (uint32_t)lit(ui.out_t, (int)p) << 1);
    h = hash(h, (uint32_t)plat_ble_state() | (uint32_t)plat_ble_connected() << 4);
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
    if (ui.view == V_SETUP) {
        h = hash(hash(h, plat_ble_log_total()), ui.n_in[PORT_BLE] ^ ui.n_out[PORT_BLE] << 16);
        for (i = 0; i < 7; i++)
            h = hash_s(h, plat_ble_log_line(i));
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
                                                 ui.sustain << 12 | proj.arp_on << 13 | proj.arp_latch << 14 |
                                                 (uint32_t)(eng_mod_val() != 0) << 15),
                           proj.program | proj.sound << 8 | (uint32_t)proj.chan << 12), ui.view);
    if (h == ui.sig[3])
        return;
    ui.sig[3] = h;
    cv_begin(240, STAT_H, K_BG);
    cv_rect(8, 0, 224, 1, K_LINE);
    put_note(put_s(t, "Lowest "), proj.scale ? SCALE0_NOTE + proj.root + 12 * proj.octave : KEY0_NOTE + 12 * proj.octave);
    x = cv_text(8, 3, &FONT_XS, t, K_DIM) + 8;
    if (proj.arp_on)
        x += tag(x, 2, proj.arp_latch ? "LATCH" : "ARP", K_HOT_T, K_HOT) + 4;
    if (ui.sustain)
        x += tag(x, 2, "SUSTAIN", K_HOT_T, K_HOT) + 4;
    if (eng_mod_val())
        tag(x, 2, "MOD", K_HOT_T, K_HOT);
    prog_name(t, proj.program);
    cv_text(232 - text_w(&FONT_XS, t), 3, &FONT_XS, t, K_TEXT);
    cv_blit(0, STAT_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8 | proj.bank << 16 | ui.track << 20 |
                                       (uint32_t)fx_cc() << 24), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(hash(h, (uint32_t)knob_get(view_knob((int)i))), proj.cc[proj.bank][i]);
    h = hash(hash(h, (uint32_t)now_touch), (uint32_t)plat_ble_state());
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_round(4, 2, 232, KNB_H - 6, 6, K_PANEL);
    for (i = 0; i < 4; i++) {
        int k = view_knob((int)i), lo, hi, v;
        int32_t x = (int32_t)i * 60, cx = x + 30, bw;
        int hot = now_touch && ui.touched == (int)i;
        char t[16];
        if (i)
            cv_rect(x, 14, 1, KNB_H - 30, K_LINE);
        if (k == K_NONE)
            continue;
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        knob_name(k, t);
        text_c(cx, 6, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        knob_text(k, t);
        {   /* the value: big, unless it has lower case (the big face has none) or is long */
            const char *q = t;
            int lower = 0;
            for (; *q; q++)
                lower |= *q >= 'a' && *q <= 'z';
            if (lower || text_w(&FONT_M, t) > 56)
                text_c(cx, 26, text_w(&FONT_B, t) > 58 ? &FONT_XS : &FONT_B, t, hot ? K_HOT : K_TEXT);
            else
                text_c(cx, 22, &FONT_M, t, hot ? K_HOT : K_TEXT);
        }
        bw = hi > lo ? (int32_t)(44 * (v - lo) / (hi - lo)) : 0;
        cv_round(cx - 22, 50, 44, 6, 3, K_LINE);
        if (bw)
            cv_round(cx - 22, 50, bw < 6 ? 6 : bw, 6, 3, hot ? K_HOT : K_COOL);
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
    plat_leds(b, k, db, dk, proj.glow);
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

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.master_sent = -1;
    ui.master_raw = (int32_t)plat_master();
    lcd_fill(0, 0, 240, 240, K_BG);
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    plat_yield();
    draw_main();
    plat_yield();
    draw_status();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
