/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl settings: one flash object (OBJ_PROJ). Settings of another format, or none, give the defaults.
 * Part of the unity build, after app.h. */
project_t proj;

/* KNOB 1-4: filter cutoff, resonance, modulation wheel, volume (the General MIDI / common synth numbers) */
static const uint8_t CC_DEFAULT[NKNOBS] = {74, 71, 1, 7};

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.vel = 100;
    for (i = 0; i < NKNOBS; i++) {
        proj.cc[i] = CC_DEFAULT[i];
        proj.val[i] = CC_DEFAULT[i] == 7 ? 100 : 64;
    }
    proj.val[2] = 0;                             /* the mod wheel at rest */
    proj.leds = 1;
}

static int project_valid(void)
{
    int i;
    if (proj.magic != PROJ_MAGIC || proj.format != PROJ_FORMAT)
        return 0;
    if (proj.chan > 15u || proj.vel < 1u || proj.vel > 127u || proj.octave < -3 || proj.octave > 3 ||
        proj.program > 127u || proj.leds > 1u)
        return 0;
    for (i = 0; i < NKNOBS; i++)
        if (proj.cc[i] > 119u || proj.val[i] > 127u)
            return 0;
    return 1;
}

int project_load(void)
{
    int n;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void) { return plat_store_save(OBJ_PROJ, &proj, sizeof proj); }
