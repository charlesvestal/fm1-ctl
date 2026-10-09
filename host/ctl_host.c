/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl on the host: the whole app (UI, settings, MIDI routing) against a simulated FM-1 driven by a
 * script. The screen goes to PNGs, the lights to text, MIDI out to a log (one line per message and
 * port). The device-only parts (HAL, USB, TRS, BLE stack, OTA, flash driver) are replaced by plat.h
 * implemented here; the Bluetooth stack is a stand-in that starts in a second. (From FoMni's.)
 *
 *   ctl_host SCRIPT [OUTDIR]
 *
 * Script, one command per line (# comments):
 *   wait MS                      run the device for MS milliseconds
 *   press BTN | release BTN | tap BTN     (BTN: FX SEL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+)
 *   key K down|up | tapkey K     (K: 0..26 = F3..G5)
 *   turn ENC N [MS]              (ENC: SELECT ALGO PRESET K1 K2 K3 K4; N detents, MS apart, default 80)
 *   spin ENC N                   N detents at once, exactly N (no acceleration)
 *   midi PORT B0 B1 B2           incoming MIDI message (PORT: USB BLE TRS; hex bytes)
 *   master N                     MASTER pot 0..4096
 *   ble connect | ble disconnect a central (the simulated stack must be ready)
 *   shot FILE.png                save the screen
 *   leds                         print the lit buttons and keys (keys: # bright, + dim)
 *   out                          print the MIDI sent since the last "out" (and forget it; clocks not shown)
 *   expect WHAT VALUE            check state: exit 1 on mismatch; VALUE "<N" / ">N" is a bound
 *   expect_out PORT B0 B1 B2     check that this message went out to PORT since the last "out"
 *   reboot                       re-run boot from the simulated flash (persistence test)
 */
#define CT_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define __attribute__(x)
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------ screen --- */
static uint16_t fb[240 * 240];          /* RGB565, byte-swapped as the panel takes it (gfx.c) */
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sc = (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8));
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = sc;
}
static uint32_t blits;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    blits++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = px[j * w + i];
}
#include "../firmware/src/gfx.c"


/* ---------------------------------------------------------- platform --- */
#include "../firmware/src/app/plat.h"

static uint32_t now_ms, held_btn, held_keys, master = 0;
static int32_t enc_acc[NE];
static uint32_t lit_btn, lit_keys, dim_btn, dim_keys;
static int glow_level;

uint32_t plat_ms(void) { return now_ms; }
uint32_t plat_buttons(void) { return held_btn; }
uint32_t plat_keys(void) { return held_keys; }
int32_t plat_enc(int role)
{
    int32_t v = enc_acc[role];
    enc_acc[role] = 0;
    return v;
}
uint32_t plat_master(void) { return master; }
void plat_leds(uint32_t b, uint32_t k, uint32_t db, uint32_t dk, int glow)
{
    lit_btn = b;
    lit_keys = k;
    dim_btn = glow ? db & ~b : 0;
    dim_keys = glow ? dk & ~k : 0;
    glow_level = glow;
}
void plat_yield(void) {}
uint32_t plat_lock(void) { return 0; }
void plat_unlock(uint32_t f) { (void)f; }

/* MIDI: one queue in (tagged with the port), a record of what went out */
#define MQ 256
static uint32_t min_q[MQ], min_port[MQ], mi_w, mi_r;
static FILE *midi_log;
static uint32_t out_pkt[4096], out_to[4096], n_out, midi_out_count;
static int ble_state, ble_conn;
static uint32_t ble_t0;
int plat_midi_in(uint32_t *pkt, int *port)
{
    if (mi_r == mi_w)
        return 0;
    *pkt = min_q[mi_r % MQ];
    *port = (int)min_port[mi_r % MQ];
    mi_r++;
    return 1;
}
static void host_synth(uint32_t pkt, uint32_t to);
void plat_midi_send(uint32_t pkt, uint32_t to)
{
    host_synth(pkt, to);                   /* the onboard synth, as the device */
    to &= TO_USB | TO_BLE;
    if ((to & TO_BLE) && !ble_conn)       /* as the device: nothing queues for a central not there */
        to &= ~TO_BLE;
    if (!to)
        return;
    midi_out_count++;
    if (n_out < 4096) {
        out_pkt[n_out] = pkt;
        out_to[n_out++] = to;
    }
    if (midi_log)
        fprintf(midi_log, "%u ms: %02X %02X %02X ->%s%s\n", now_ms, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF,
                (pkt >> 24) & 0xFF, to & TO_USB ? " USB" : "", to & TO_BLE ? " BLE" : "");
}

/* Bluetooth: a stand-in for the stack (ready a second after the start) and its log */
static char ble_log[8][40];
static uint32_t ble_log_n;
static void ble_say(const char *s)
{
    snprintf(ble_log[ble_log_n++ % 8], 40, "%s", s);
}
int plat_ble_state(void) { return ble_state; }
int plat_ble_connected(void) { return ble_conn; }
void plat_ble_start(void)
{
    if (ble_state != BLE_OFF || !CT_BLE)  /* the USB-only build: no Bluetooth */
        return;
    ble_state = BLE_STARTING;
    ble_t0 = now_ms;
    ble_log_n = 0;
    ble_say("BT: starting");
}
static void ble_tick(void)
{
    if (ble_state == BLE_STARTING && now_ms - ble_t0 >= 1000u) {
        ble_state = BLE_READY;
        ble_say("BT init ok: starting BLE");
    }
}
uint32_t plat_ble_log_total(void) { return ble_log_n; }
const char *plat_ble_log_line(int back)
{
    if (back < 0 || (uint32_t)back >= ble_log_n || back >= 8)
        return "";
    return ble_log[(ble_log_n - 1u - (uint32_t)back) % 8];
}

/* flash: one buffer per object, kept across a simulated reboot */
static uint8_t store[OBJ_NOBJ][PLAT_STORE_MAX];
static int store_len[OBJ_NOBJ] = {-1, -1, -1};
static uint32_t store_writes;
int plat_store_load(uint32_t obj, void *dst, uint32_t max)
{
    int n;
    if (obj >= OBJ_NOBJ || (n = store_len[obj]) < 0)
        return -1;
    if ((uint32_t)n > max)
        n = (int)max;
    memcpy(dst, store[obj], (size_t)n);
    return n;
}
int plat_store_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_NOBJ || len > PLAT_STORE_MAX)
        return -1;
    memcpy(store[obj], src, len);
    store_len[obj] = (int)len;
    store_writes++;
    return 0;
}

/* -------------------------------------------------------------- app --- */
#include "../firmware/src/app/app.h"
#include "../firmware/src/app/project.c"
#include "../firmware/src/app/engine.c"
#include "../firmware/src/app/ui.c"

/* ---------------------------------------------------------- outputs --- */
static void png_write(const char *path)
{
    /* RGB PNG with stored (uncompressed) deflate blocks: no zlib needed */
    static uint8_t raw[240 * (1 + 240 * 3)];
    FILE *f = fopen(path, "wb");
    uint32_t crc_t[256], i, j, k, n = 0, a = 1, b = 0, pos, len = sizeof raw;
    uint8_t hdr[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    for (i = 0; i < 256; i++) {
        uint32_t c = i;
        for (k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_t[i] = c;
    }
    for (j = 0; j < 240; j++) {
        raw[n++] = 0;
        for (i = 0; i < 240; i++) {
            uint16_t p = fb[j * 240 + i];
            p = (uint16_t)((p >> 8) | (p << 8));
            raw[n++] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            raw[n++] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            raw[n++] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    for (i = 0; i < len; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
#define PUT32(v) do { uint8_t q_[4] = {(uint8_t)((v) >> 24), (uint8_t)((v) >> 16), (uint8_t)((v) >> 8), (uint8_t)(v)}; fwrite(q_, 1, 4, f); } while (0)
    fwrite(hdr, 1, 8, f);
    {
        uint8_t ih[17] = {'I', 'H', 'D', 'R', 0, 0, 0, 240, 0, 0, 0, 240, 8, 2, 0, 0, 0};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(13u);
        fwrite(ih, 1, 17, f);
        for (i = 0; i < 17; i++)
            c = crc_t[(c ^ ih[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    {
        uint32_t nblk = (len + 65534) / 65535, zlen = 2 + len + nblk * 5 + 4, c = 0xFFFFFFFFu;
        uint8_t *z = malloc(zlen + 4), *q = z;
        memcpy(q, "IDAT", 4);
        q += 4;
        *q++ = 0x78;
        *q++ = 0x01;
        for (pos = 0; pos < len;) {
            uint32_t m = len - pos > 65535 ? 65535 : len - pos;
            *q++ = pos + m >= len ? 1 : 0;
            *q++ = (uint8_t)m;
            *q++ = (uint8_t)(m >> 8);
            *q++ = (uint8_t)~m;
            *q++ = (uint8_t)(~m >> 8);
            memcpy(q, raw + pos, m);
            q += m;
            pos += m;
        }
        *q++ = (uint8_t)(b >> 8);
        *q++ = (uint8_t)b;
        *q++ = (uint8_t)(a >> 8);
        *q++ = (uint8_t)a;
        PUT32(zlen);
        fwrite(z, 1, zlen + 4, f);
        for (i = 0; i < zlen + 4; i++)
            c = crc_t[(c ^ z[i]) & 255] ^ (c >> 8);
        PUT32(~c);
        free(z);
    }
    {
        uint8_t ie[4] = {'I', 'E', 'N', 'D'};
        uint32_t c = 0xFFFFFFFFu;
        PUT32(0u);
        fwrite(ie, 1, 4, f);
        for (i = 0; i < 4; i++)
            c = crc_t[(c ^ ie[i]) & 255] ^ (c >> 8);
        PUT32(~c);
    }
    fclose(f);
}

/* --------------------------------------------------------- the device --- */
static uint32_t ui_due;

uint32_t plat_cpu_pct(void) { return 0; }
static void host_synth(uint32_t pkt, uint32_t to)
{
    uint32_t st = (pkt >> 8) & 0xFFu;
    if (((to & TO_SYNTH) || ((to & (TO_USB | TO_BLE)) && !(to & TO_NOSYNTH))) && proj.sound != SOUND_OFF &&
        st >= 0x80u && st < 0xF0u)
        gm_midi(st, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
}

static void boot(void)
{
    static int synth;
    if (!synth)
        synth = gm_init() == 0;
    project_load();
    lcd_fill(0, 0, 240, 240, 0);
    ble_state = BLE_OFF;                   /* Bluetooth starts at every boot, as on the device */
    ble_conn = 0;
    ui_init();
    plat_ble_start();
}

static void run_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms; i++) {
        now_ms++;
        ble_tick();
        engine_tick();                     /* the device's 1 kHz tick */
        if (now_ms % 6u == 0u) {           /* the synth, about as often as the device's audio halves */
            static int32_t blk[256 * 2];
            gm_render(blk, 256, 4096);
        }
        if (now_ms >= ui_due) {
            ui_due = now_ms + 16;
            ui_frame();
        } else {
            ui_input_only();
        }
    }
}

/* ---------------------------------------------------------- script --- */
static const char *const BTN_N[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                      "PLAY", "REC", "OCT-", "OCT+"};
static const char *const ENC_N[NE] = {"SELECT", "ALGO", "PRESET", "K1", "K2", "K3", "K4"};
static const char *const PORT_N[NPORTS] = {"USB", "BLE", "TRS"};

static int find_name(const char *const *t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}

static uint32_t mk_pkt(const char *a, const char *b, const char *c)
{
    uint32_t s = (uint32_t)strtoul(a, 0, 16), d1 = b[0] ? (uint32_t)strtoul(b, 0, 16) : 0,
             d2 = c[0] ? (uint32_t)strtoul(c, 0, 16) : 0;
    uint32_t cin = s >= 0xF0 ? 0x0F : s >> 4;
    return cin | s << 8 | d1 << 16 | d2 << 24;
}

static int expect(const char *what, const char *val)
{
    int got;
    if (!strcmp(what, "voices"))           /* the onboard synth's voices sounding */
        got = gm_voices;
    else if (!strcmp(what, "view"))
        got = ui.view;
    else if (!strcmp(what, "chan"))
        got = proj.chan;
    else if (!strcmp(what, "vel"))
        got = proj.vel;
    else if (!strcmp(what, "octave"))
        got = proj.octave;
    else if (!strcmp(what, "program"))
        got = proj.program;
    else if (!strcmp(what, "dirty"))
        got = ui.dirty;
    else if (!strcmp(what, "sustain"))
        got = ui.sustain;
    else if (!strcmp(what, "playing"))
        got = ui.playing;
    else if (!strcmp(what, "store_writes"))
        got = (int)store_writes;
    else if (!strcmp(what, "midi_out"))
        got = (int)midi_out_count;
    else if (!strcmp(what, "ble"))
        got = ble_state;
    else if (!strcmp(what, "bank"))
        got = proj.bank;
    else if (!strcmp(what, "tempo"))
        got = proj.tempo;
    else if (!strcmp(what, "arp"))
        got = proj.arp_on;
    else if (!strcmp(what, "scale"))
        got = proj.scale;
    else if (!strcmp(what, "track"))
        got = ui.track;
    else if (!strncmp(what, "pat", 3))            /* patN: track N's step bits */
        got = proj.trk[atoi(what + 3) & 3].pat;
    else if (!strncmp(what, "mute", 4))
        got = proj.trk[atoi(what + 4) & 3].mute;
    else if (!strcmp(what, "clocks_out")) {       /* MIDI clocks sent since the last "out" */
        uint32_t i;
        got = 0;
        for (i = 0; i < n_out; i++)
            got += ((out_pkt[i] >> 8) & 0xFF) == 0xF8;
    } else if (!strcmp(what, "notes_out")) {     /* note-ons sent since the last "out" */
        uint32_t i;
        got = 0;
        for (i = 0; i < n_out; i++)
            got += ((out_pkt[i] >> 8) & 0xF0) == 0x90 && (out_pkt[i] >> 24);
    } else if (!strcmp(what, "notes_on")) {      /* note-ons minus note-offs since the last "out" */
        uint32_t i;
        got = 0;
        for (i = 0; i < n_out; i++) {
            uint32_t st = (out_pkt[i] >> 8) & 0xF0;
            got += st == 0x90 && (out_pkt[i] >> 24) ? 1 : st == 0x80 || st == 0x90 ? -1 : 0;
        }
    }
    else if (!strncmp(what, "cc", 2))             /* ccN: knob N's controller number */
        got = proj.cc[proj.bank][atoi(what + 2) & 3];
    else if (!strncmp(what, "val", 3))            /* valN: knob N's value */
        got = proj.val[proj.bank][atoi(what + 3) & 3];
    else {
        printf("FAIL unknown expect %s\n", what);
        return 1;
    }
    if (val[0] == '<' || val[0] == '>') {
        int lim = (int)strtol(val + 1, 0, 0);
        if (val[0] == '<' ? got >= lim : got <= lim) {
            printf("FAIL expect %s %s, got %d\n", what, val, got);
            return 1;
        }
        printf("  ok %s == %d (%s)\n", what, got, val);
        return 0;
    }
    if (got != (int)strtol(val, 0, 0)) {
        printf("FAIL expect %s == %s, got %d\n", what, val, got);
        return 1;
    }
    printf("  ok %s == %d\n", what, got);
    return 0;
}

static int expect_out(const char *port, uint32_t pkt)
{
    int p = find_name(PORT_N, NPORTS, port);
    uint32_t i;
    for (i = 0; i < n_out; i++)
        if (out_pkt[i] == pkt && p >= 0 && (out_to[i] >> p & 1u)) {
            printf("  ok out %s %02X %02X %02X\n", port, (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, pkt >> 24);
            return 0;
        }
    printf("FAIL no %02X %02X %02X out to %s\n", (pkt >> 8) & 0xFF, (pkt >> 16) & 0xFF, pkt >> 24, port);
    return 1;
}

#ifndef CT_WEB
int main(int argc, char **argv)
{
    FILE *sc;
    char line[512], out[400];
    const char *dir = argc > 2 ? argv[2] : ".";
    int lineno = 0, fails = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: ctl_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    sc = fopen(argv[1], "r");
    if (!sc) {
        perror(argv[1]);
        return 2;
    }
    snprintf(out, sizeof out, "%s/midi_out.txt", dir);
    midi_log = fopen(out, "w");
    boot();
    while (fgets(line, sizeof line, sc)) {
        char cmd[32] = {0}, a[256] = {0}, b[64] = {0}, c[64] = {0}, d[64] = {0};
        lineno++;
        if (line[0] == '#' || sscanf(line, "%31s %255s %63s %63s %63s", cmd, a, b, c, d) < 1)
            continue;
        if (!strcmp(cmd, "wait"))
            run_ms((uint32_t)atoi(a));
        else if (!strcmp(cmd, "press") || !strcmp(cmd, "release") || !strcmp(cmd, "tap")) {
            int i = find_name(BTN_N, NB, a);
            if (i < 0) {
                printf("line %d: no button %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] != 'r')
                held_btn |= 1u << i;
            if (cmd[0] == 't')
                run_ms(40);
            if (cmd[0] != 'p')
                held_btn &= ~(1u << i);
            run_ms(cmd[0] == 't' ? 40 : 2);
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "tapkey")) {
            int k = atoi(a);
            if (k < 0 || k >= NKEYS) {
                printf("line %d: no key %s\n", lineno, a);
                return 2;
            }
            if (cmd[0] == 't') {
                held_keys |= 1u << k;
                run_ms(60);
                held_keys &= ~(1u << k);
                run_ms(30);
            } else {
                if (!strcmp(b, "down"))
                    held_keys |= 1u << k;
                else
                    held_keys &= ~(1u << k);
                run_ms(2);
            }
        } else if (!strcmp(cmd, "turn")) {
            int e = find_name(ENC_N, NE, a), n = atoi(b), s = n > 0 ? 1 : -1;
            uint32_t gap = c[0] ? (uint32_t)atoi(c) : 80u;
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            for (; n; n -= s) {
                enc_acc[e] += s;
                run_ms(gap);
            }
        } else if (!strcmp(cmd, "spin")) {
            int e = find_name(ENC_N, NE, a);
            if (e < 0) {
                printf("line %d: no encoder %s\n", lineno, a);
                return 2;
            }
            enc_acc[e] += atoi(b);
            accel_off = 1;
            run_ms(1);
            accel_off = 0;
        } else if (!strcmp(cmd, "master")) {
            master = (uint32_t)atoi(a);
            run_ms(40);
        } else if (!strcmp(cmd, "midi")) {
            int p = find_name(PORT_N, NPORTS, a);
            if (p < 0) {
                printf("line %d: no port %s\n", lineno, a);
                return 2;
            }
            min_q[mi_w % MQ] = mk_pkt(b, c, d);
            min_port[mi_w++ % MQ] = (uint32_t)p;
            run_ms(2);
        } else if (!strcmp(cmd, "ble")) {
            if (!strcmp(a, "connect") && ble_state == BLE_READY) {
                ble_conn = 1;
                ble_say("connected");
            } else if (!strcmp(a, "disconnect")) {
                ble_conn = 0;
                ble_say("disconnected: reason 13");
            } else {
                printf("line %d: ble %s: the stack is not ready\n", lineno, a);
                fails++;
            }
            run_ms(2);
        } else if (!strcmp(cmd, "shot")) {
            snprintf(out, sizeof out, "%s/%s", dir, a);
            png_write(out);
        } else if (!strcmp(cmd, "leds")) {
            int i;
            printf("  leds: buttons");
            for (i = 0; i < NB; i++)
                if (lit_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | dim");
            for (i = 0; i < NB; i++)
                if (dim_btn >> i & 1u)
                    printf(" %s", BTN_N[i]);
            printf(" | keys ");
            for (i = 0; i < NKEYS; i++)
                printf("%c", (lit_keys >> i & 1u) ? '#' : (dim_keys >> i & 1u) ? '+' : '.');
            printf("\n");
        } else if (!strcmp(cmd, "out")) {
            uint32_t i;
            for (i = 0; i < n_out; i++)
                if (((out_pkt[i] >> 8) & 0xFF) != 0xF8)
                    printf("  out %02X %02X %02X ->%s%s\n", (out_pkt[i] >> 8) & 0xFF, (out_pkt[i] >> 16) & 0xFF,
                       out_pkt[i] >> 24, out_to[i] & TO_USB ? " USB" : "", out_to[i] & TO_BLE ? " BLE" : "");
            n_out = 0;
        } else if (!strcmp(cmd, "expect"))
            fails += expect(a, b);
        else if (!strcmp(cmd, "expect_out"))
            fails += expect_out(a, mk_pkt(b, c, d));
        else if (!strcmp(cmd, "reboot")) {
            memset(&proj, 0, sizeof proj);
            boot();
        } else if (!strcmp(cmd, "echo"))
            printf("%s", line + 5);
        else {
            printf("line %d: unknown command %s\n", lineno, cmd);
            return 2;
        }
    }
    if (midi_log)
        fclose(midi_log);
    printf("host: %u ms simulated, %u blits, %u MIDI messages out%s\n", now_ms, blits, midi_out_count,
           fails ? ", EXPECTATIONS FAILED" : "");
    return fails ? 1 : 0;
}
#endif /* CT_WEB */
