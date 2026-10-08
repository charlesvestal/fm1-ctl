/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl app: the settings (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"

#ifndef CT_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define CT_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x4C544331u           /* "CTL1" */
#define PROJ_FORMAT 1u
#define NKNOBS 4

typedef struct {
    uint32_t magic, format;
    uint8_t chan;                        /* MIDI channel 0..15 (shown 1..16) */
    uint8_t vel;                         /* note-on velocity 1..127 */
    int8_t octave;                       /* -3..+3: key 0 is F3 at 0 */
    uint8_t program;                     /* the last program change sent, 0..127 */
    uint8_t cc[NKNOBS];                  /* KNOB 1-4: controller numbers 0..119 */
    uint8_t val[NKNOBS];                 /* KNOB 1-4: their values 0..127 */
    uint8_t leds;                        /* key lights on */
    uint8_t rsv[15];                     /* room to grow: an older, shorter settings object loads (zeros here) */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
