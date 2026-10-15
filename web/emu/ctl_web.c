/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl in the browser: the host simulator (host/ctl_host.c: the whole app against a simulated FM-1)
 * compiled to WebAssembly and run in an AudioWorklet (from FoMni's). The worklet's render quanta are its
 * clock: each one runs the device for the milliseconds it covers, then the GM synth renders the quantum's
 * sound (MASTER as on the device: a squared curve, or full when it sends a controller). The page sends
 * input and MIDI in, and reads the screen, the lights, the saved objects and the MIDI sent back
 * through these exports (the page can pass that MIDI on to a port with Web MIDI).
 *
 *   web/emu/build.sh   ->  build/emu/ctl.wasm (+ the page) */
#include <stdint.h>
#include "../../host/ctl_host.c"

#undef __attribute__

static float out_l[1024], out_r[1024];
__attribute__((used, visibility("default"))) float *web_out_l(void) { return out_l; }
__attribute__((used, visibility("default"))) float *web_out_r(void) { return out_r; }

/* n frames at 44.1 kHz (<= 1024): the device runs that long, then the synth sounds them. The synth
 * renders in the device's blocks of 256 frames (EAS's 128 at 22.05 kHz, whole), into a little queue the
 * quanta (128 frames in a browser) take from. */
#define WB 256u
static uint32_t frac;
static int32_t q_lr[(1024u + WB) * 2u];
static uint32_t q_n;                        /* frames waiting in q_lr */
__attribute__((used, visibility("default"))) void web_render(uint32_t n)
{
    uint32_t i, g = proj.master_cc ? 4096u : (master * master) >> 12;
    frac += n * 1000u;
    while (frac >= 44100u) {
        frac -= 44100u;
        run_ms(1);
    }
    if (n > 1024u)
        n = 1024u;
    while (q_n < n) {
        if (proj.sound == SOUND_OFF)
            gm_skip(q_lr + 2u * q_n, WB);
        else
            gm_render(q_lr + 2u * q_n, WB, g);
        q_n += WB;
    }
    for (i = 0; i < n; i++) {
        out_l[i] = (float)q_lr[2 * i] * (1.0f / 8388608.0f);
        out_r[i] = (float)q_lr[2 * i + 1] * (1.0f / 8388608.0f);
    }
    q_n -= n;
    for (i = 0; i < 2u * q_n; i++)
        q_lr[i] = q_lr[2u * n + i];
}

/* a screen reader (fm1-reader): the page writes a SysEx frame (the bytes between F0 and F7) to web_sysex_buf and
 * calls web_sysex(n); what the firmware says back, whole F0..F7 frames, from web_said_buf (web_said_len bytes;
 * web_said_clear when read) */
static uint8_t said_buf[4096], sx_in[64];
static uint32_t said_n;
static void web_said(const uint8_t *p, uint32_t n)
{
    if (said_n + n <= sizeof said_buf) {
        memcpy(said_buf + said_n, p, n);
        said_n += n;
    }
}
__attribute__((used, visibility("default"))) uint8_t *web_said_buf(void) { return said_buf; }
__attribute__((used, visibility("default"))) uint32_t web_said_len(void) { return said_n; }
__attribute__((used, visibility("default"))) void web_said_clear(void) { said_n = 0; }
__attribute__((used, visibility("default"))) uint8_t *web_sysex_buf(void) { return sx_in; }
__attribute__((used, visibility("default"))) void web_sysex(uint32_t n) { ann_frame(sx_in, n > sizeof sx_in ? sizeof sx_in : n, now_ms); }

__attribute__((used, visibility("default"))) void web_boot(void) { boot(); }
__attribute__((used, visibility("default"))) void web_buttons(uint32_t m) { held_btn = m; }
/* the keys held (a mask), each landing at velocity 100 when it goes down; web_key: one key, at vel 1..127
 * (0 = up), pressed press 0..1000 more (Touch on: sense.c reads them as the FM-1's keys) */
__attribute__((used, visibility("default"))) void web_keys(uint32_t m)
{
    uint32_t k, ch = m ^ held_keys;
    for (k = 0; k < NKEYS; k++)
        if (ch >> k & 1u)
            host_key(k, m >> k & 1u ? 100u : 0u, 0.0f);
}
__attribute__((used, visibility("default"))) void web_key(uint32_t k, uint32_t vel, uint32_t press)
{
    host_key(k, vel, (float)(press > 1000u ? 1000u : press) * 0.001f);
}
__attribute__((used, visibility("default"))) void web_enc(int role, int32_t n)
{
    if (role >= 0 && role < NE)
        enc_acc[role] += n;
}
__attribute__((used, visibility("default"))) uint32_t web_lit_buttons(void) { return lit_btn; }
__attribute__((used, visibility("default"))) uint32_t web_lit_keys(void) { return lit_keys; }
__attribute__((used, visibility("default"))) uint32_t web_dim_buttons(void) { return dim_btn; }
__attribute__((used, visibility("default"))) uint32_t web_dim_keys(void) { return dim_keys; }
__attribute__((used, visibility("default"))) void web_master(uint32_t v) { master = v > 4096u ? 4096u : v; }
__attribute__((used, visibility("default"))) uint16_t *web_fb(void) { return fb; }
__attribute__((used, visibility("default"))) uint32_t web_blits(void) { return blits; }
__attribute__((used, visibility("default"))) uint32_t web_now(void) { return now_ms; }
__attribute__((used, visibility("default"))) uint32_t web_voices(void) { return gm_voices; }   /* the synth's, for the tests */

/* MIDI: in from the page (a Web MIDI input, as USB), out as the device sent it (USB-MIDI packets) */
__attribute__((used, visibility("default"))) void web_midi_in(uint32_t port, uint32_t pkt)
{
    if (port < NPORTS && mi_w - mi_r < MQ) {
        min_q[mi_w % MQ] = pkt;
        min_port[mi_w++ % MQ] = port;
    }
}
__attribute__((used, visibility("default"))) uint32_t web_out_n(void) { return n_out; }
__attribute__((used, visibility("default"))) uint32_t *web_out_pkts(void) { return out_pkt; }
__attribute__((used, visibility("default"))) void web_out_clear(void) { n_out = 0; }

/* saved objects: the page keeps them (localStorage) and gives them back before boot */
__attribute__((used, visibility("default"))) uint8_t *web_store(uint32_t obj) { return obj < OBJ_NOBJ ? store[obj] : 0; }
__attribute__((used, visibility("default"))) int web_store_len(uint32_t obj) { return obj < OBJ_NOBJ ? store_len[obj] : -1; }
__attribute__((used, visibility("default"))) void web_store_set_len(uint32_t obj, int n)
{
    if (obj < OBJ_NOBJ)
        store_len[obj] = n < 0 ? -1 : n > (int)PLAT_STORE_MAX ? (int)PLAT_STORE_MAX : n;
}
__attribute__((used, visibility("default"))) uint32_t web_store_writes(void) { return store_writes; }
__attribute__((used, visibility("default"))) uint32_t web_nobj(void) { return OBJ_NOBJ; }
__attribute__((used, visibility("default"))) uint32_t web_store_max(void) { return PLAT_STORE_MAX; }
