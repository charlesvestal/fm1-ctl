/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl in the browser: the host simulator (host/ctl_host.c: the whole app against a simulated FM-1)
 * compiled to WebAssembly and run in an AudioWorklet (from FoMni's). Ctl makes no sound; the worklet's
 * render quanta are its clock: each one runs the device for the milliseconds it covers. The page sends
 * input and MIDI in, and reads the screen, the lights, the saved objects and the MIDI sent back
 * through these exports (the page can pass that MIDI on to a port with Web MIDI).
 *
 *   web/emu/build.sh   ->  build/emu/ctl.wasm (+ the page) */
#include <stdint.h>
#include "../../host/ctl_host.c"

#undef __attribute__

static float out_l[1024], out_r[1024];      /* silence */
__attribute__((used, visibility("default"))) float *web_out_l(void) { return out_l; }
__attribute__((used, visibility("default"))) float *web_out_r(void) { return out_r; }

/* n frames at 44.1 kHz: the device runs that long */
static uint32_t frac;
__attribute__((used, visibility("default"))) void web_render(uint32_t n)
{
    frac += n * 1000u;
    while (frac >= 44100u) {
        frac -= 44100u;
        run_ms(1);
    }
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
__attribute__((used, visibility("default"))) uint16_t *web_fb(void) { return fb; }
__attribute__((used, visibility("default"))) uint32_t web_blits(void) { return blits; }
__attribute__((used, visibility("default"))) uint32_t web_now(void) { return now_ms; }

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
