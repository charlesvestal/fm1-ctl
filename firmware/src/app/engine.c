/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl engine: the clock (internal, or followed from MIDI in), the arpeggiator and the four-track
 * sequencer. engine_tick runs every millisecond from the platform's timer: on the device that is the
 * TIMER5 interrupt, so the timing holds while the main loop draws or Bluetooth works. The main loop
 * changes the arpeggiator's notes under plat_lock(); everything else it shares is a single byte or
 * halfword. Part of the unity build after project.c; the host simulator includes it too.
 *
 *   clock     24 ticks a quarter note. Internal: from proj.tempo, sent out (CLK_INT_OUT) or not.
 *             External: one tick per MIDI clock received (engine_midi_clock, from the main loop).
 *   arp       runs whenever it is on and has notes, playing or not; rate 1/4 .. 1/32, gate half a step
 *   sequencer runs while playing (PLAY, or MIDI start when external): sixteenths, each track its own
 *             length (polymetric), gate an eighth of a beat */

static const uint8_t RATE_TICKS[NRATE] = {24, 12, 8, 6, 4, 3};
#define SEQ_TICKS 6u                     /* a sixteenth */
#define SEQ_GATE 3u

static struct {
    uint32_t phase;                      /* internal clock: += tempo * 24 a millisecond, a tick at 60000 */
    uint32_t ticks;                      /* since boot (the beat light) */
    volatile uint8_t running;            /* the sequencer plays */
    uint32_t seq_tick;                   /* ticks since the start */
    uint32_t seq_step;
    uint8_t seq_note[NTRACKS], seq_left[NTRACKS];   /* the note each track has on (0xFF none), ticks to its off */
    /* the arpeggiator's notes: in the order played; held = the keys down now */
    uint8_t pool[16], npool;
    uint32_t held[4];
    uint32_t arp_tick;
    int arp_pos;
    uint8_t arp_note, arp_left;          /* the note on (0xFF none), ticks to its off */
    uint32_t rnd;
} eng = {.arp_note = 0xFF, .seq_note = {0xFF, 0xFF, 0xFF, 0xFF}, .rnd = 2463534242u};

static uint32_t eng_rand(void)
{
    eng.rnd ^= eng.rnd << 13;
    eng.rnd ^= eng.rnd >> 17;
    eng.rnd ^= eng.rnd << 5;
    return eng.rnd;
}

static uint32_t eng_vel(void)                    /* the velocity, spread by Humanize */
{
    int v = proj.vel;
    if (proj.human)
        v += (int)(eng_rand() % (2u * proj.human + 1u)) - proj.human;
    return (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v);
}

static void eng_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    plat_midi_send((st >> 4) | st << 8 | d1 << 16 | d2 << 24, TO_USB | TO_BLE);
}

/* ------------------------------------------------------------ arpeggiator --- */
static int arp_seq(uint8_t *out)                 /* the notes in playing order, octaves included */
{
    uint8_t s[16];
    int n = eng.npool, i, j, o, k = 0;
    for (i = 0; i < n; i++)
        s[i] = eng.pool[i];
    if (proj.arp_mode != ARP_PLAYED)             /* sorted, low to high */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && s[j - 1] > s[j]; j--) {
                uint8_t t = s[j];
                s[j] = s[j - 1];
                s[j - 1] = t;
            }
    for (o = 0; o < proj.arp_oct; o++)
        for (i = 0; i < n; i++)
            if (s[i] + 12 * o < 128)
                out[k++] = (uint8_t)(s[i] + 12 * o);
    return k;
}

static void arp_off(void)
{
    if (eng.arp_note != 0xFF) {
        eng_send(0x80u | proj.chan, eng.arp_note, 0);
        eng.arp_note = 0xFF;
    }
}

static void arp_step(void)
{
    uint8_t q[48];
    int n = arp_seq(q), i;
    arp_off();
    if (!n)
        return;
    switch (proj.arp_mode) {
    case ARP_DOWN: i = n - 1 - eng.arp_pos % n; break;
    case ARP_UPDOWN: {                           /* up, then down, the ends once */
        int per = n > 1 ? 2 * n - 2 : 1, p = eng.arp_pos % per;
        i = p < n ? p : per - p;
        break;
    }
    case ARP_RANDOM: i = (int)(eng_rand() % (uint32_t)n); break;
    default: i = eng.arp_pos % n; break;
    }
    eng.arp_pos++;
    eng.arp_note = q[i];
    eng.arp_left = (uint8_t)(RATE_TICKS[proj.arp_rate] / 2u);
    eng_send(0x90u | proj.chan, q[i], eng_vel());
}

static int held_has(int n) { return (int)(eng.held[n >> 5] >> (n & 31) & 1u); }
static int held_any(void) { return (eng.held[0] | eng.held[1] | eng.held[2] | eng.held[3]) != 0; }

void arp_press(int n)                            /* main loop */
{
    uint32_t f = plat_lock();
    int i;
    if (proj.arp_latch && !held_any())          /* latched: a new chord replaces the last */
        eng.npool = 0;
    if (!held_any() && !eng.npool)
        eng.arp_pos = 0;
    eng.held[n >> 5] |= 1u << (n & 31);
    for (i = 0; i < eng.npool && eng.pool[i] != n; i++)
        ;
    if (i == eng.npool && eng.npool < 16)
        eng.pool[eng.npool++] = (uint8_t)n;
    plat_unlock(f);
}

static void pool_keep_held(void)                 /* under the lock */
{
    int i, k = 0;
    for (i = 0; i < eng.npool; i++)
        if (held_has(eng.pool[i]))
            eng.pool[k++] = eng.pool[i];
    eng.npool = (uint8_t)k;
}

void arp_release(int n)
{
    uint32_t f = plat_lock();
    eng.held[n >> 5] &= ~(1u << (n & 31));
    if (!proj.arp_latch)
        pool_keep_held();
    plat_unlock(f);
}

void arp_latch_changed(void)
{
    uint32_t f = plat_lock();
    if (!proj.arp_latch)
        pool_keep_held();
    plat_unlock(f);
}

void arp_set(int on)
{
    uint32_t f = plat_lock();
    proj.arp_on = (uint8_t)on;
    if (!on) {
        arp_off();
        eng.npool = 0;
        memset(eng.held, 0, sizeof eng.held);
    }
    plat_unlock(f);
}

/* -------------------------------------------------------------- sequencer --- */
static void seq_offs(void)
{
    int t;
    for (t = 0; t < NTRACKS; t++)
        if (eng.seq_note[t] != 0xFF) {
            eng_send(0x80u | proj.seq_ch, eng.seq_note[t], 0);
            eng.seq_note[t] = 0xFF;
        }
}

static void seq_step(void)
{
    int t;
    for (t = 0; t < NTRACKS; t++) {
        const track_t *k = &proj.trk[t];
        uint32_t pos = eng.seq_step % k->steps;
        if (k->mute || !(k->pat >> pos & 1u))
            continue;
        if (eng.seq_note[t] != 0xFF)
            eng_send(0x80u | proj.seq_ch, eng.seq_note[t], 0);
        eng.seq_note[t] = k->note;
        eng.seq_left[t] = SEQ_GATE;
        eng_send(0x90u | proj.seq_ch, k->note, eng_vel());
    }
    eng.seq_step++;
}

/* --------------------------------------------------------------- the clock --- */
static void clock_tick(void)                     /* one 24th of a beat */
{
    int t;
    eng.ticks++;
    if (eng.arp_note != 0xFF && eng.arp_left && --eng.arp_left == 0)
        arp_off();
    for (t = 0; t < NTRACKS; t++)
        if (eng.seq_note[t] != 0xFF && eng.seq_left[t] && --eng.seq_left[t] == 0) {
            eng_send(0x80u | proj.seq_ch, eng.seq_note[t], 0);
            eng.seq_note[t] = 0xFF;
        }
    if (proj.arp_on && eng.npool) {
        if (eng.arp_tick % RATE_TICKS[proj.arp_rate] == 0u)
            arp_step();
        eng.arp_tick++;
    } else {
        eng.arp_tick = 0;
        if (!eng.npool)
            arp_off();
    }
    if (eng.running) {
        if (eng.seq_tick % SEQ_TICKS == 0u)
            seq_step();
        eng.seq_tick++;
    }
}

void engine_tick(void)                           /* every millisecond (device: TIMER5 interrupt) */
{
    if (proj.clock == CLK_EXT)
        return;
    eng.phase += proj.tempo * 24u;
    while (eng.phase >= 60000u) {
        eng.phase -= 60000u;
        if (proj.clock == CLK_INT_OUT)
            plat_midi_send(0xFu | 0xF8u << 8, TO_USB | TO_BLE);
        clock_tick();
    }
}

static void seq_start(int from_top)
{
    if (from_top) {
        eng.seq_tick = 0;
        eng.seq_step = 0;
    }
    eng.running = 1;
}

static void seq_stop(void)
{
    eng.running = 0;
    seq_offs();
}

void engine_midi_clock(uint32_t st)              /* main loop: from MIDI in */
{
    uint32_t f;
    if (proj.clock != CLK_EXT)
        return;
    f = plat_lock();
    if (st == 0xF8u)
        clock_tick();
    else if (st == 0xFAu)
        seq_start(1);
    else if (st == 0xFBu)
        seq_start(0);
    else if (st == 0xFCu)
        seq_stop();
    plat_unlock(f);
}

void engine_play(int on)                         /* main loop: PLAY */
{
    uint32_t f = plat_lock();
    if (on) {
        if (proj.clock != CLK_EXT)
            plat_midi_send(0xFu | 0xFAu << 8, TO_USB | TO_BLE);
        eng.phase = 60000u - proj.tempo * 24u;   /* the first tick right away */
        seq_start(1);
    } else {
        if (proj.clock != CLK_EXT)
            plat_midi_send(0xFu | 0xFCu << 8, TO_USB | TO_BLE);
        seq_stop();
    }
    plat_unlock(f);
}

void engine_panic(void)
{
    uint32_t f = plat_lock();
    arp_off();
    seq_offs();
    eng.npool = 0;
    memset(eng.held, 0, sizeof eng.held);
    plat_unlock(f);
}

/* for the screen and the lights */
static int eng_running(void) { return eng.running; }
static int eng_beat_on(void) { return eng.ticks % 24u < 6u; }
static int eng_track_pos(int t) { return eng.running && eng.seq_step ? (int)((eng.seq_step - 1u) % proj.trk[t].steps) : -1; }
static int eng_arp_note(void) { return eng.arp_note == 0xFF ? -1 : eng.arp_note; }
static int eng_pool(uint8_t *out)
{
    int i;
    for (i = 0; i < eng.npool; i++)
        out[i] = eng.pool[i];
    return eng.npool;
}
