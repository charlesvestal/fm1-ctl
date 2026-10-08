/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl UI: a MIDI controller. Part of the unity build (after gfx.c and project.c); the host simulator
 * includes it too.
 *
 *   keys           notes, chromatic, F3..G5 at octave 0     OCT- / OCT+   the octave (-3..+3)
 *   KNOB 1-4       four controllers (CC numbers on the Setup page)
 *   SELECT         velocity          ALGORITHM   MIDI channel          PRESETS   program change
 *   ARP            sustain (held)    ENV / LFO   pitch bend down / up (held)
 *   PLAY           MIDI start / stop REC         panic (all notes off)
 *   HOME GLO SEL   the pages: Play, Setup, Bluetooth (EDIT steps through them)
 *   SAVE           saves (it also saves by itself, a few seconds after a change)
 *
 * Out: everything played goes to USB and Bluetooth. Thru: USB in -> Bluetooth, Bluetooth in -> USB,
 * the TRS jack in -> USB and Bluetooth. */

enum { V_PLAY, V_SETUP, V_BLE, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Play", "Setup", "Bluetooth"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_GLO, B_SEL};
static const char *const PORT_NAME[NPORTS] = {"USB", "BLE", "TRS"};
static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
#define KEY0_NOTE 53                   /* F3: key 0 at octave 0 */
#define FLASH_MS 120u                  /* a port's light after a message */

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

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t sent[NKEYS];               /* the note each held key sent (its note-off must match), 0xFF none */
    uint32_t heard[4];                 /* notes held from MIDI in (any port), for the key lights */
    uint8_t playing;                   /* MIDI start sent, stop not yet */
    uint8_t sustain, bend;             /* bend: 0 centre, 1 down, 2 up */
    uint8_t dirty;
    uint32_t act_t;
    uint32_t in_t[NPORTS], out_t[NPORTS];   /* when each port last carried a message */
    uint32_t n_in[NPORTS], n_out[NPORTS];
    char last[24];                     /* the last message played here, in words */
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
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

/* ------------------------------------------------------------- MIDI --- */
static uint32_t pkt3(uint32_t st, uint32_t d1, uint32_t d2) { return (st >> 4) | st << 8 | d1 << 16 | d2 << 24; }
static uint32_t pkt_rt(uint32_t st) { return 0xFu | st << 8; }

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

static void key_down(int k)
{
    int n = KEY0_NOTE + k + 12 * proj.octave;
    char *b;
    if (n < 0 || n > 127)
        return;
    ui.sent[k] = (uint8_t)n;
    play(0x90u, (uint32_t)n, proj.vel);
    b = put_note(put_s(ui.last, "Note "), n);
    put_u(put_s(b, "  "), proj.vel);
}

static void key_up(int k)
{
    if (ui.sent[k] == 0xFFu)
        return;
    play(0x80u, ui.sent[k], 0);
    ui.sent[k] = 0xFFu;
}

static void bend_to(uint32_t how)        /* 0 centre, 1 down, 2 up */
{
    uint32_t v = how == 1u ? 0u : how == 2u ? 16383u : 8192u;
    if (how == ui.bend)
        return;
    ui.bend = (uint8_t)how;
    play(0xE0u, v & 0x7Fu, v >> 7);
    put_s(ui.last, how == 1u ? "Bend down" : how == 2u ? "Bend up" : "Bend centre");
}

static void panic(void)
{
    int k;
    for (k = 0; k < NKEYS; k++)
        key_up(k);
    play(0xB0u, 64, 0);
    play(0xB0u, 123, 0);                 /* all notes off */
    ui.sustain = 0;
    bend_to(0);
    put_s(ui.last, "All notes off");
}

/* MIDI in: forwarded (USB -> BLE, BLE -> USB, TRS -> both), noted for the lights */
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
        send(pkt & ~0xF0u, THRU[port]);  /* cable 0 */
        if ((st & 0xF0u) == 0x90u && v)
            ui.heard[n >> 5] |= 1u << (n & 31u);
        else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u)
            ui.heard[n >> 5] &= ~(1u << (n & 31u));
        else if ((st & 0xF0u) == 0xB0u && (n == 123u || n == 120u))
            memset(ui.heard, 0, sizeof ui.heard);
    }
}

/* ------------------------------------------------------------ values --- */
/* the four knobs of each page: one of these, with the knob's index in the low bits */
enum { K_VAL = 0, K_CC = 8, K_BT = 16, K_LEDS = 17, K_NONE = 255 };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {K_VAL + 0, K_VAL + 1, K_VAL + 2, K_VAL + 3},
    {K_CC + 0, K_CC + 1, K_CC + 2, K_CC + 3},
    {K_BT, K_LEDS, K_NONE, K_NONE},
};

static int knob_get(int k)
{
    if (k < K_CC)
        return proj.val[k - K_VAL];
    if (k < K_BT)
        return proj.cc[k - K_CC];
    if (k == K_BT)
        return plat_ble_state() != BLE_OFF;
    if (k == K_LEDS)
        return proj.leds;
    return 0;
}
static int knob_hi(int k) { return k < K_CC ? 127 : k < K_BT ? 119 : 1; }
static void knob_name(int k, char *b)
{
    if (k < K_CC) {
        put_u(put_s(b, "CC "), proj.cc[k - K_VAL]);
    } else if (k < K_BT) {
        put_u(put_s(b, "Knob "), (uint32_t)(k - K_CC + 1));
    } else {
        put_s(b, k == K_BT ? "Bluetooth" : k == K_LEDS ? "Key lights" : "");
    }
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (k == K_BT) {
        static const char *const S[] = {"Off", "On", "On", "Failed"};
        put_s(b, S[plat_ble_state() & 3]);
    } else if (k == K_LEDS) {
        put_s(b, v ? "On" : "Off");
    } else if (k >= K_CC && k < K_BT) {
        put_u(put_s(b, "#"), (uint32_t)v);
    } else {
        put_u(b, (uint32_t)v);
    }
}

static void mark_dirty(void) { ui.dirty = 1; }

static void knob_set(int k, int v)
{
    int hi = knob_hi(k);
    v = v < 0 ? 0 : v > hi ? hi : v;
    if (v == knob_get(k))
        return;
    if (k < K_CC) {
        proj.val[k - K_VAL] = (uint8_t)v;
        cc(proj.cc[k - K_VAL], (uint32_t)v);
    } else if (k < K_BT) {
        proj.cc[k - K_CC] = (uint8_t)v;
    } else if (k == K_BT) {
        if (v && plat_ble_state() == BLE_OFF) {
            plat_ble_start();
            say("BLUETOOTH", "STARTING");
        } else if (!v) {
            say("BLUETOOTH STOPS", "AT A RESTART");
        }
        return;                          /* not saved: Bluetooth is off at every boot */
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
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
    put_note(t, KEY0_NOTE + 12 * o);
    say("LOWEST KEY", t);
    mark_dirty();
}

static void button_down(int b)
{
    switch (b) {
    case B_ARP:
        ui.sustain = 1;
        cc(64, 127);
        break;
    case B_ENV: case B_LFO:              /* both held: the one pressed last */
        bend_to(b == B_ENV ? 1u : 2u);
        break;
    case B_PLAY:
        ui.playing = (uint8_t)!ui.playing;
        send(pkt_rt(ui.playing ? 0xFAu : 0xFCu), TO_USB | TO_BLE);
        put_s(ui.last, ui.playing ? "Start" : "Stop");
        break;
    case B_REC: panic(); break;
    case B_OCTDN: octave(-1); break;
    case B_OCTUP: octave(1); break;
    default: break;
    }
}

static void button_up(int b)
{
    switch (b) {
    case B_ARP:
        ui.sustain = 0;
        cc(64, 0);
        break;
    case B_ENV: case B_LFO: {
        uint32_t m = ui.btn >> B_ENV & 3u;   /* what is still held */
        bend_to(m == 1u ? 1u : m == 2u ? 2u : 0u);
        break;
    }
    case B_HOME: set_view(V_PLAY); break;
    case B_GLO: set_view(V_SETUP); break;
    case B_SEL: set_view(V_BLE); break;
    case B_EDIT: set_view((ui.view + 1) % NVIEWS); break;
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

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = keys ^ ui.keys;                 /* keys first: a note is the most urgent thing here */
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if (ch >> i & 1u) {
            if (keys >> i & 1u)
                key_down((int)i);
            else
                key_up((int)i);
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
    if ((e = plat_enc(EN_SELECT)) != 0) {
        int v = proj.vel + accel(EN_SELECT, e, 127);
        proj.vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
        mark_dirty();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        int c = clamp(proj.chan + e, 0, 15);
        if (c != proj.chan) {
            int k;
            for (k = 0; k < NKEYS; k++)  /* held notes end on the channel they started on */
                key_up(k);
            proj.chan = (uint8_t)c;
            mark_dirty();
        }
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        int p = clamp(proj.program + e, 0, 127);
        if (p != proj.program) {
            proj.program = (uint8_t)p;
            play(0xC0u, (uint32_t)p, 0);
            put_u(put_s(ui.last, "Program "), (uint32_t)p + 1u);
            mark_dirty();
        }
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            int k = VIEW_KNOB[ui.view][i];
            if (k != K_NONE)
                knob_set(k, knob_get(k) + accel(EN_K1 + (int)i, e, knob_hi(k)));
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = plat_ms();
        }
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the page (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
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

static int lit(const uint32_t *t, int p) { return plat_ms() - t[p] < FLASH_MS && t[p]; }

static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until, bs = plat_ble_state(), bc = plat_ble_connected();
    uint32_t h = hash(hash(hash(2166136261u, ui.view), (uint32_t)msg | ui.dirty << 1 | (uint32_t)bs << 2 |
                                                           (uint32_t)bc << 4 | ui.playing << 5),
                      proj.chan | (uint32_t)(proj.octave + 3) << 8);
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
        int32_t x = cv_text(10, 6, &FONT_B, VIEW_NAME[ui.view], K_TEXT);
        if (ui.dirty)
            cv_round(x + 6, 12, 5, 5, 2, K_DIM);
        x = 232;                         /* right: the channel, then Bluetooth's state */
        put_u(put_s(t, "CH "), proj.chan + 1u);
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, K_TEXT);
        if (bs != BLE_OFF) {
            uint16_t c = bc ? K_COOL : bs == BLE_FAILED ? K_HOT : bs == BLE_READY ? K_COOL_T :
                         (ui.frame / 8u) & 1u ? K_COOL : K_COOL_T;
            int32_t w = text_w(&FONT_XS, "BLE") + 10;
            x -= w + 8;
            cv_round(x, 6, w, 16, 6, c);
            cv_text(x + 5, 7, &FONT_XS, "BLE", bc ? K_WHITE : K_TEXT);
        }
    }
    cv_blit(0, 0);
}

/* the keyboard: 16 white keys, 11 black, as the FM-1's; held keys coral, notes heard teal */
static const uint8_t WHITE_K[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static int key_heard(int k)
{
    int n = KEY0_NOTE + k + 12 * proj.octave;
    return n >= 0 && n < 128 && (ui.heard[n >> 5] >> (n & 31) & 1u);
}
static void draw_keys(int32_t y0, int32_t h)
{
    int i, k;
    cv_round(3, y0 - 1, 16 * 14 + 1, h + 2, 3, K_LINE);
    for (i = 0; i < 16; i++) {
        int32_t x = 4 + i * 14;
        k = WHITE_K[i];
        cv_round(x, y0, 13, h, 2, ui.keys >> k & 1u ? K_HOT : key_heard(k) ? K_COOL_T : K_KEYW);
    }
    for (i = 0; i < 16; i++) {           /* a black key after each white one that has one */
        int32_t x = 4 + i * 14 + 9;
        k = WHITE_K[i] + 1;
        if (i == 15 || WHITE_K[i + 1] != k + 1)
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
    char t[24];
    int32_t x;
    cv_text(12, 4, &FONT_B, ui.last[0] ? ui.last : "Play a key", ui.last[0] ? K_TEXT : K_DIM);
    draw_keys(30, 46);
    put_note(t, KEY0_NOTE + 12 * proj.octave);
    cv_text(4, 78, &FONT_XS, t, K_DIM);
    put_note(t, KEY0_NOTE + 12 * proj.octave + NKEYS - 1);
    cv_text(236 - text_w(&FONT_XS, t), 78, &FONT_XS, t, K_DIM);
    x = 120;                             /* centre: velocity and program */
    {
        char a[16], b[16];
        put_u(put_s(a, "vel "), proj.vel);
        put_u(put_s(b, "prog "), proj.program + 1u);
        text_c(x - 40, 78, &FONT_XS, a, K_DIM);
        text_c(x + 40, 78, &FONT_XS, b, K_DIM);
    }
    if (ui.sustain || ui.bend) {
        const char *s = ui.sustain ? (ui.bend ? "SUS+BEND" : "SUSTAIN") : "BEND";
        int32_t w = text_w(&FONT_XS, s) + 10;
        cv_round(232 - w, 6, w, 16, 6, K_HOT_T);
        cv_text(232 - w + 5, 7, &FONT_XS, s, K_HOT);
    }
    draw_ports(100);
}

static void draw_setup(void)
{
    static const char *const L[] = {"SELECT  velocity", "ALGORITHM  channel", "PRESETS  program",
                                    "ARP  sustain    ENV / LFO  bend", "PLAY  start / stop    REC  panic",
                                    "OCT- / OCT+  octave"};
    int i;
    cv_text(12, 4, &FONT_S, "KNOB 1-4: their controller numbers", K_TEXT);
    for (i = 0; i < 6; i++)
        cv_text(12, 26 + i * 17, &FONT_XS, L[i], K_DIM);
    cv_text(12, 26 + 6 * 17, &FONT_XS, "Firmware " CT_VERSION, K_LINE);
}

static void draw_ble(void)
{
    char t[40];
    int s = plat_ble_state(), i, n;
    const char *big = plat_ble_connected() ? "Connected" : s == BLE_READY ? "Advertising" :
                      s == BLE_STARTING ? "Starting" : s == BLE_FAILED ? "Failed" : "Off";
    cv_text(12, 2, &FONT_B, big, plat_ble_connected() ? K_COOL : K_TEXT);
    if (s == BLE_OFF) {
        cv_text(12, 22, &FONT_XS, "KNOB 1 starts it, until a restart", K_DIM);
    } else {
        char *b = put_u(put_s(t, "Name: FM-1    in "), ui.n_in[PORT_BLE]);
        put_u(put_s(b, "   out "), ui.n_out[PORT_BLE]);
        cv_text(12, 22, &FONT_XS, t, K_DIM);
    }
    n = (int)plat_ble_log_total();       /* the log, the newest last (kept through a reset): seven lines, */
    if (n > 7)                           /* the whole crash note */
        n = 7;
    for (i = 0; i < n; i++)
        cv_text(8, 40 + (6 - (n - 1 - i)) * 14, &FONT_XS, plat_ble_log_line(n - 1 - i), K_DIM);
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, ui.view), p;
    int i;
    h = hash(h, ui.keys);
    h = hash(hash(h, ui.heard[0] ^ ui.heard[1] * 3u ^ ui.heard[2] * 5u ^ ui.heard[3] * 7u),
             proj.vel | proj.program << 8 | (uint32_t)(proj.octave + 3) << 16 | ui.sustain << 20 | ui.bend << 21);
    h = hash_s(h, ui.last);
    for (p = 0; p < NPORTS; p++)
        h = hash(h, (uint32_t)lit(ui.in_t, (int)p) | (uint32_t)lit(ui.out_t, (int)p) << 1);
    h = hash(h, (uint32_t)plat_ble_state() | (uint32_t)plat_ble_connected() << 4);
    if (ui.view == V_BLE) {
        h = hash(hash(h, plat_ble_log_total()), ui.n_in[PORT_BLE] ^ ui.n_out[PORT_BLE] << 16);
        for (i = 0; i < 7; i++)
            h = hash_s(h, plat_ble_log_line(i));
    }
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (ui.view == V_PLAY)
        draw_play();
    else if (ui.view == V_SETUP)
        draw_setup();
    else
        draw_ble();
    cv_blit(0, MAIN_Y);
}

static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(hash(h, (uint32_t)knob_get(VIEW_KNOB[ui.view][i])), proj.cc[i]);
    h = hash(hash(h, (uint32_t)now_touch), (uint32_t)plat_ble_state());
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    cv_round(4, 2, 232, KNB_H - 6, 6, K_PANEL);
    for (i = 0; i < 4; i++) {
        int k = VIEW_KNOB[ui.view][i], hi, v;
        int32_t x = (int32_t)i * 60, cx = x + 30, bw;
        int hot = now_touch && ui.touched == (int)i;
        char t[16];
        if (i)
            cv_rect(x, 14, 1, KNB_H - 30, K_LINE);
        if (k == K_NONE)
            continue;
        hi = knob_hi(k);
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
                text_c(cx, 26, &FONT_B, t, hot ? K_HOT : K_TEXT);
            else
                text_c(cx, 22, &FONT_M, t, hot ? K_HOT : K_TEXT);
        }
        bw = (int32_t)(44 * v / hi);
        cv_round(cx - 22, 50, 44, 6, 3, K_LINE);
        if (bw)
            cv_round(cx - 22, 50, bw < 6 ? 6 : bw, 6, 3, hot ? K_HOT : K_COOL);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 1u << VIEW_BTN[ui.view], k = 0, i;
    if (ui.playing)
        b |= 1u << B_PLAY;
    if (ui.sustain)
        b |= 1u << B_ARP;
    if (proj.octave < 0)
        b |= 1u << B_OCTDN;
    if (proj.octave > 0)
        b |= 1u << B_OCTUP;
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if ((ui.keys >> i & 1u) || key_heard((int)i))
                k |= 1u << i;
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, when nothing is held or touched (a flash erase stops
 * everything for a moment) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET)
        return;
    if (project_save() == 0)
        ui.dirty = 0;
}

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    memset(ui.sent, 0xFF, sizeof ui.sent);
    ui.touched = -1;
    lcd_fill(0, 0, 240, 240, K_BG);
}

void ui_frame(void)
{
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    ui.frame++;
}

void ui_input_only(void) { input(); }
