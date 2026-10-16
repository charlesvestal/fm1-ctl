/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The GM module (gm.h): Sonivox EAS behind a MIDI queue, its 22.05 kHz doubled to 44.1. One unit, -O2. */
#include "gm.h"
#include "eas/eas.h"
#include "eas/eas_data.h"
#include "eas/eas_synth.h"

#define BARRIER() __asm__ volatile("" ::: "memory")
#define EAS_FRAMES BUFFER_SIZE_IN_MONO_SAMPLES        /* 128 at 22.05 kHz: half of the FM-1's 256 */

volatile uint8_t gm_voices;
volatile uint8_t gm_program[GM_NCH];
volatile float gm_level;

static EAS_DATA_HANDLE eas;
static EAS_HANDLE stream;
static int ready;
static int skipped;                               /* gm_skip ran: every note off at the next render */
static int poly = GM_VOICES;                      /* the polyphony now: gm_overload lowers it */

/* the queue: 3-byte messages, the main loop writes, the render reads */
#define QN 256u
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
void gm_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;
    q[w % QN] = (st & 0xFFu) | (d1 & 0x7Fu) << 8 | (d2 & 0x7Fu) << 16;
    BARRIER();
    q_w = w + 1;
}
void gm_panic(void)
{
    for (uint32_t ch = 0; ch < GM_NCH; ch++) {
        gm_midi(0xB0u | ch, 123, 0);              /* all notes off */
        gm_midi(0xB0u | ch, 120, 0);              /* all sound off */
    }
}

/* what gm_skip set aside: programs, controllers and bends, in order (a full one keeps the first) */
#define HELD_N 64u
static uint32_t held[HELD_N];
static uint32_t held_n;

static void drain(void)
{
    for (uint32_t i = 0; i < held_n; i++) {
        uint32_t m = held[i];
        EAS_U8 b[3] = {(EAS_U8)m, (EAS_U8)(m >> 8), (EAS_U8)(m >> 16)};
        if ((m & 0xF0u) == 0xC0u)
            gm_program[b[0] & 15u] = b[1];
        EAS_WriteMIDIStream(eas, stream, b, (m & 0xF0u) == 0xC0u ? 2 : 3);
    }
    held_n = 0;
    while (q_r != q_w) {
        uint32_t m = q[q_r % QN];
        EAS_U8 b[3] = {(EAS_U8)m, (EAS_U8)(m >> 8), (EAS_U8)(m >> 16)};
        uint32_t st = b[0] & 0xF0u;
        BARRIER();
        q_r++;
        if (st == 0xC0u)
            gm_program[b[0] & 15u] = b[1];
        EAS_WriteMIDIStream(eas, stream, b, (st == 0xC0u || st == 0xD0u) ? 2 : 3);
    }
}

/* Gain staging. EAS mixes its voices in 32 bits, then hard-clips to 16 at its master volume, before our
 * gain: at its default 0 dB, six loud notes reached the rail (ten: hundreds of samples a chord). So EAS runs
 * 6 dB down (ten notes at velocity 127 peak near -3 dBFS there), and the 24-bit stage here makes the 6 dB up,
 * through a soft knee above -2.5 dBFS instead of a hard edge. */
#define GM_EAS_VOLUME 94                          /* EAS volume: 100 = 0 dB, 1 dB a step */
#define FS24 8388607                              /* 24-bit full scale */
#define KNEE 6291456                              /* 0.75 of it */
static inline int32_t soft24(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    float d, y;
    if (a <= KNEE)
        return x;
    d = (float)(a - KNEE) * (1.f / (float)(FS24 - KNEE));
    y = (float)KNEE + (float)(FS24 - KNEE) * d / (1.f + d);   /* slope 1 at the knee, never past full scale */
    return x < 0 ? -(int32_t)y : (int32_t)y;
}

int gm_init(void)
{
    if (EAS_Init(&eas) != EAS_SUCCESS || EAS_OpenMIDIStream(eas, &stream, NULL) != EAS_SUCCESS)
        return -1;
    EAS_SetSynthPolyphony(eas, 0, poly);          /* not the library's 64: ~2.5% CPU a voice */
    EAS_SetVolume(eas, NULL, GM_EAS_VOLUME);
    ready = 1;
    return 0;
}

/* 2x up: each EAS sample and the halfway point to the next (linear), one sample late */
static EAS_PCM pcm[EAS_FRAMES * 2];
static int32_t prev_l, prev_r;
void gm_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const int32_t g = (int32_t)gain_q12;          /* Q12 */
    float peak = gm_level * 0.97f;
    if (!ready) {
        for (uint32_t i = 0; i < 2 * n; i++)
            out[i] = 0;
        return;
    }
    if (skipped) {                                /* a note-off may have been dropped meanwhile */
        skipped = 0;
        for (uint32_t ch = 0; ch < GM_NCH; ch++) {
            EAS_U8 off[3] = {(EAS_U8)(0xB0u | ch), 120, 0};   /* all sound off */
            EAS_WriteMIDIStream(eas, stream, off, 3);
        }
    }
    drain();
    while (n >= 2) {
        uint32_t k = n / 2 > EAS_FRAMES ? EAS_FRAMES : n / 2, i;
        EAS_I32 got = 0;
        EAS_Render(eas, pcm, (EAS_I32)k, &got);
        for (i = 0; i < k; i++) {
            int32_t l = (i < (uint32_t)got) ? pcm[2 * i] : 0, r = (i < (uint32_t)got) ? pcm[2 * i + 1] : 0;
            int32_t ml = (l + prev_l) >> 1, mr = (r + prev_r) >> 1;
            int32_t a = l < 0 ? -l : l;
            if ((float)a * (1.f / 16384.f) > peak)    /* after the 6 dB made up, MASTER full */
                peak = (float)a * (1.f / 16384.f);
            out[0] = soft24((ml * g) >> 3);       /* 16-bit, gain Q12, to 24-bit (<< 8 >> 12), +6 dB */
            out[1] = soft24((mr * g) >> 3);
            out[2] = soft24((l * g) >> 3);
            out[3] = soft24((r * g) >> 3);
            out += 4;
            prev_l = l;
            prev_r = r;
        }
        n -= 2 * k;
    }
    gm_level = peak;
    gm_voices = (uint8_t)(eas->pVoiceMgr ? eas->pVoiceMgr->activeVoices : 0);
}

void gm_overload(void)
{
    if (ready && poly > GM_VOICES_MIN) {
        poly -= 4;
        EAS_SetSynthPolyphony(eas, 0, poly);      /* the oldest voices are stolen */
    }
}

void gm_skip(int32_t *out, uint32_t n)
{
    for (uint32_t i = 0; i < 2 * n; i++)
        out[i] = 0;
    while (q_r != q_w) {                          /* the notes dropped; programs and controllers kept for */
        uint32_t m = q[q_r % QN], st = m & 0xF0u;     /* the next render (EAS untouched here) */
        BARRIER();
        q_r++;
        if ((st == 0xB0u || st == 0xC0u || st == 0xE0u) && held_n < HELD_N)
            held[held_n++] = m;
        if (st == 0xC0u)
            gm_program[m & 15u] = (uint8_t)((m >> 8) & 0x7Fu);   /* what the channel will play, known now */
    }
    skipped = 1;
    gm_level = 0;
    gm_voices = 0;
}
