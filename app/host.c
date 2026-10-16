/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Ctl in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): what the keys' sensing
 * would read, Ctl's checks for "expect", its commands, a reboot and the screen reader's dump.
 *
 * Ctl's checks: voices sound view setup_pg touch last_vel at chan vel octave program synth_program dirty sustain
 * sus_lock playing bank tempo arp scale track patN muteN ccN valN, and clocks_out notes_out notes_on (since the
 * script's last "out"). Its command: fixed (Touch off: the keys at the fixed velocity, no aftertouch; Touch is on
 * by default, and scenarios about the notes tap too quickly to be sensed). */

/* ------------------------------------------------- the keys, sensed --- */
/* with Touch on, what Velo's sensing would read from the FM-1 (MONK-1's emulator): a pulled-up sample under
 * PRESS, a driven one at the depth the key's calibration maps to its velocity, then deeper by the lean that ui.c's
 * pressure() reads as its pressure; the top seven keys through the probe */
#define PULL_DOWN 300u
#define ANALOG 0xFFFFFu                  /* keys 0..19 on the analog rows (plat_analog_mask) */
static uint8_t k_vel[NKEYS];             /* 0 = up; else the velocity it lands with */
static float k_press[NKEYS];             /* 0..1 */
static uint16_t k_land[NKEYS];
static uint32_t k_t0[NKEYS];

static uint16_t land_for(uint32_t k, uint32_t v)   /* sense_velocity_d, inverted */
{
    const sense_key_t *s = &sn_key[k];
    float r0 = (float)(s->dfloor > SN_DBASE ? s->dfloor - SN_DBASE : 1u), r1 = (float)(s->dceil - SN_DBASE);
    float c = (float)(sn_cfg.d_curve ? sn_cfg.d_curve : 1u) * 0.01f, t;
    if (v >= 127u)
        return (uint16_t)(s->dfloor > 1u ? s->dfloor - 1u : 1u);
    t = 1.0f - fm_powf(((float)v - 1.0f) / 126.0f, 1.0f / c);
    return (uint16_t)(SN_DBASE + (uint32_t)(r0 * fm_powf(r1 / r0, t) + 0.5f));
}

void app_host_tick(void)                 /* every 2 ms: each analog key's samples */
{
    uint32_t k, now = plat_ms();
    if (now & 1u)
        return;
    for (k = 0; k < NKEYS; k++) {
        if (!(ANALOG >> k & 1u))
            continue;
        if (!k_vel[k]) {
            if (sn_key[k].st != SN_OPEN)
                sense_sample(k, 1023u, now * 24000u, 0);
            continue;
        }
        if (sn_key[k].st == SN_OPEN) {
            k_land[k] = land_for(k, k_vel[k]);
            k_t0[k] = 0;
        }
        sense_sample(k, PULL_DOWN, now * 24000u, 0);
        if (sense_want_drive(k)) {
            uint32_t d = k_land[k];
            if (sn_key[k].st == SN_DOWN && !k_t0[k])
                k_t0[k] = now;
            if (k_t0[k] && now - k_t0[k] > P_SETTLE + 20u && p_on[k] && k_press[k] > 0.0f) {
                float lean = P_DEAD + k_press[k] * P_SPAN(0.5f + (float)proj.at_sens * 0.09f);
                float r = p_base[k] / fm_exp2f(lean);
                d = SN_DBASE + (uint32_t)(r > 2.0f ? r : 2.0f);
            }
            sense_sample(k, d, now * 24000u, 1);
        }
    }
}

/* key k: vel 0 = up, else held, landed at vel; press 0..100 % */
void app_host_key(uint32_t k, uint32_t vel, uint32_t press)
{
    if (k >= NKEYS)
        return;
    k_vel[k] = (uint8_t)(vel > 127u ? 127u : vel);
    k_press[k] = (float)(press > 100u ? 100u : press) * 0.01f;
    if (vel && !(ANALOG >> k & 1u)) {    /* the probe's rise above its open reading that maps to vel (the curve inverted) */
        uint32_t lo = sn_cfg.t_lo, hi = sn_cfg.t_hi > lo ? sn_cfg.t_hi : lo + 1u, i;
        float c = (float)(sn_cfg.t_curve ? sn_cfg.t_curve : sn_cfg.d_curve ? sn_cfg.d_curve : 1u) * 0.01f;
        float x = fm_powf(((float)k_vel[k] - 1.0f) / 126.0f, 1.0f / c);
        for (i = 0; i < 8u; i++)
            sense_probe(k, 100u, 0);
        sense_probe(k, 100u + lo + (uint32_t)((float)(hi - lo) * x + 0.5f), 1);
    } else if (!(ANALOG >> k & 1u)) {
        sense_probe(k, 100u, 0);
    }
}

/* ------------------------------------------------------------ checks --- */
static int out_count(int what)           /* since the script's last "out": 0 clocks, 1 note-ons, 2 notes still on */
{
    uint32_t i;
    int n = 0;
    for (i = 0; i < n_out; i++) {
        uint32_t st = (out_pkt[i] >> 8) & 0xFFu, on = (st & 0xF0u) == 0x90u && (out_pkt[i] >> 24);
        if (what == 0)
            n += st == 0xF8u;
        else if (what == 1)
            n += (int)on;
        else
            n += on ? 1 : (st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u ? -1 : 0;
    }
    return n;
}

int app_expect(const char *what, int *got)
{
    int g, k;
    if (!strcmp(what, "voices"))         /* the onboard synth's voices sounding */
        g = gm_voices;
    else if (!strcmp(what, "sound"))
        g = proj.sound;
    else if (!strcmp(what, "view"))
        g = ui.view;
    else if (!strcmp(what, "setup_pg"))  /* Setup's pages: 1 = Touch */
        g = ui.setup_pg;
    else if (!strcmp(what, "touch"))     /* the keys are sensed (velocity or aftertouch on) */
        g = touch_live;
    else if (!strcmp(what, "last_vel"))  /* the last velocity sensed */
        g = ui.last_vel;
    else if (!strcmp(what, "at")) {      /* the most aftertouch sent for a held key */
        g = 0;
        for (k = 0; k < NKEYS; k++)
            g = at_sent[k] > g ? at_sent[k] : g;
    } else if (!strcmp(what, "chan"))
        g = proj.chan;
    else if (!strcmp(what, "vel"))
        g = proj.vel;
    else if (!strcmp(what, "octave"))
        g = proj.octave;
    else if (!strcmp(what, "program"))
        g = proj.program;
    else if (!strcmp(what, "synth_program"))   /* what the onboard synth plays on the channel */
        g = gm_program[proj.chan];
    else if (!strcmp(what, "dirty"))
        g = ui.dirty;
    else if (!strcmp(what, "sustain"))
        g = ui.sustain;
    else if (!strcmp(what, "sus_lock"))
        g = ui.sus_lock;
    else if (!strcmp(what, "playing"))
        g = ui.playing;
    else if (!strcmp(what, "bank"))
        g = proj.bank;
    else if (!strcmp(what, "tempo"))
        g = proj.tempo;
    else if (!strcmp(what, "arp"))
        g = proj.arp_on;
    else if (!strcmp(what, "scale"))
        g = proj.scale;
    else if (!strcmp(what, "track"))
        g = ui.track;
    else if (!strncmp(what, "pat", 3))   /* patN: track N's step bits */
        g = proj.trk[atoi(what + 3) & 3].pat;
    else if (!strncmp(what, "mute", 4))
        g = proj.trk[atoi(what + 4) & 3].mute;
    else if (!strcmp(what, "clocks_out"))
        g = out_count(0);
    else if (!strcmp(what, "notes_out"))
        g = out_count(1);
    else if (!strcmp(what, "notes_on"))
        g = out_count(2);
    else if (!strncmp(what, "cc", 2))    /* ccN: knob N's controller number */
        g = proj.cc[proj.bank][atoi(what + 2) & 3];
    else if (!strncmp(what, "val", 3))   /* valN: knob N's value */
        g = proj.val[proj.bank][atoi(what + 3) & 3];
    else
        return 0;
    *got = g;
    return 1;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)a, (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "fixed")) {         /* the keys at the fixed velocity, no aftertouch */
        proj.touch = 0;
        proj.at_mode = AT_OFF;
        touch_apply();
        return 1;
    }
    return 0;
}

/* a restart: the settings from the simulated flash again, the synth as at power-on (program 0 everywhere) */
void app_host_reboot(void)
{
    int32_t silence[512];
    uint32_t ch;
    memset(&proj, 0, sizeof proj);
    for (ch = 0; ch < 16; ch++) {
        gm_midi(0xC0u | ch, 0, 0);
        gm_midi(0xB0u | ch, 121, 0);
    }
    gm_panic();
    gm_render(silence, 256, 0);
}

void app_speak_dump(void) { speak_dump(); }

#ifdef PLAT_WEB
#undef __attribute__
__attribute__((used, visibility("default"))) uint32_t web_voices(void) { return gm_voices; }   /* for the tests */
#define __attribute__(x)
#endif
