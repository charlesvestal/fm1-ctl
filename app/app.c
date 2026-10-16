/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* CTL-1 on fm1-base: the app's unity build (compiled into fm1-base's, after the platform and its lib/touch.c) and
 * the glue between fm1-base's contract (app.h) and Ctl's parts: the settings (project.c), the engine (engine.c), the
 * UI (ui.c) and the GM synth (dsp/gm.c, its own -O2 unit). */
#include "project.c"
#include "engine.c"
#include "ui.c"

/* MIDI out: to the ports, and to the onboard synth (channel messages only: its queue is single-writer, and this
 * runs under the lock or in the tick; with Sound off no notes, but programs and controllers still set) */
static volatile uint8_t in_tick;
void ctl_send(uint32_t pkt, uint32_t to)
{
    uint32_t f = 0;
    if (!in_tick)
        f = plat_lock();
    if (to & (TO_USB | TO_BLE))
        plat_midi_send(pkt, to & (TO_USB | TO_BLE));
    if ((to & TO_SYNTH) || ((to & (TO_USB | TO_BLE)) && !(to & TO_NOSYNTH))) {
        uint32_t st = (pkt >> 8) & 0xFFu;
        if (st >= 0x80u && st < 0xF0u && (proj.sound != SOUND_OFF || st >= 0xB0u))
            gm_midi(st, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
    }
    if (!in_tick)
        plat_unlock(f);
}

/* ---- the contract */
static uint8_t synth_ok;
void app_init(void)
{
    project_load();                      /* defaults when nothing (or another format) is saved */
    if (!synth_ok)
        synth_ok = gm_init() == 0;       /* the onboard GM synth, once */
}
void app_render(int32_t *lr, uint32_t n, uint32_t gain_q12)
{
    if (proj.sound == SOUND_OFF)
        gm_skip(lr, n);
    else
        gm_render(lr, n, gain_q12);
}
void app_quiet(int32_t *lr, uint32_t n) { gm_skip(lr, n); }
void app_overload(void) { gm_overload(); }
void app_panic(void) { gm_panic(); }
void app_tick(void)
{
    in_tick = 1;
    engine_tick();
    in_tick = 0;
}
void app_ui_init(void)
{
    static uint8_t booted;
    ui_init();
    if (!booted && FM1B_BLE && plat_ble_state() == BLE_OFF)
        ui_say("BLUETOOTH OFF:", "IT CRASHED");   /* not twice in a row: SAVE held on Setup starts it */
    booted = 1;
}
void app_frame(void) { ui_frame(); }
void app_input_only(void) { ui_input_only(); }
void app_say(const char *a, const char *b) { ui_say(a, b); }
void app_splash(void)                    /* ui.c's paper and ink */
{
    lcd_fill(0, 0, 240, 240, RGB(251, 248, 241));
    cv_begin(240, 64, RGB(251, 248, 241));
    {
        int32_t w = text_w(&FONT_L, APP_NAME);
        cv_text((240 - w) / 2, 0, &FONT_L, APP_NAME, RGB(30, 43, 90));
        cv_rect((240 - w) / 2, 37, w, 2, RGB(229, 72, 46));
        w = text_w(&FONT_S, APP_TAGLINE);
        cv_text((240 - w) / 2, 44, &FONT_S, APP_TAGLINE, RGB(122, 128, 152));
    }
    cv_blit(0, 92);
}

#ifdef PLAT_HOST
#include "host.c"                        /* the host simulator's part: checks and commands */
#endif
