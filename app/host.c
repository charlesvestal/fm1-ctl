/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Ctl in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): Ctl's checks for "expect",
 * its commands, a reboot and the screen reader's dump. The keys' readings: fm1-base's lib/touch.c.
 *
 * Ctl's checks: voices sound view setup_pg touch last_vel at chan vel octave program synth_program dirty sustain
 * sus_lock playing bank tempo arp scale track patN muteN ccN valN, and clocks_out notes_out notes_on (since the
 * script's last "out"). Its command: fixed (Touch off: the keys at the fixed velocity, no aftertouch; Touch is on
 * by default, and scenarios about the notes tap too quickly to be sensed). */

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
    } else if (!strcmp(what, "ble_dev"))   /* the Bluetooth MIDI device: BLE_DEV_* (4 connected) */
        g = plat_ble_device();
    else if (!strcmp(what, "ble_off"))
        g = proj.ble_off;
    else if (!strcmp(what, "chan"))
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
