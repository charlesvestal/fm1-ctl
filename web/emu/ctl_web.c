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

__attribute__((used, visibility("default"))) void web_boot(void) { boot(); }
__attribute__((used, visibility("default"))) void web_buttons(uint32_t m) { held_btn = m; }
__attribute__((used, visibility("default"))) void web_keys(uint32_t m) { held_keys = m; }
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
