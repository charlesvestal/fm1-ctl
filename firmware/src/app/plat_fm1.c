/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl platform on the FM-1: plat.h over Felucca's HAL, panel map, USB rings, the TRS jack and
 * storage (Bluetooth: ble.c). Part of the unity build (main.c), after the HAL, usb.c, storage.c and
 * panel.c. */

uint32_t plat_ms(void) { return fm1_ms; }

uint32_t plat_buttons(void)
{
    uint32_t raw = fm1_in.buttons, out = 0, i;
    for (i = 0; i < NB; i++)
        if (raw & (1u << panel.btn[i]))
            out |= 1u << i;
    return out;
}

uint32_t plat_keys(void) { return fm1_in.notes & ((1u << NKEYS) - 1u); }

int32_t plat_enc(int role)
{
    if (role < 0 || role >= NE)
        return 0;
    return fm1_enc_take(panel.enc[role]) * panel.dir[role];
}

/* LEDs: the picture is built off-line and copied one byte per column (Felucca: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker) */
static uint8_t led_pos[FM1_NKEY];
static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < FM1_NKEY; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}
/* Glow: the dim lights on one scan frame in 6 (low) or 3 (high) */
void plat_leds(uint32_t buttons, uint32_t keys, uint32_t dim_buttons, uint32_t dim_keys, int glow)
{
    static const uint8_t DIV[3] = {0, 6, 3};
    uint8_t nl[FM1_NCOL] = {0}, dl[FM1_NCOL] = {0};
    uint32_t i, c;
    for (i = 0; i < NB + NKEYS; i++) {
        uint32_t id = i < NB ? panel.btn[i] : 14u + (i - NB);
        int on = i < NB ? (buttons >> i) & 1u : (keys >> (i - NB)) & 1u;
        int dim = i < NB ? (dim_buttons >> i) & 1u : (dim_keys >> (i - NB)) & 1u;
        uint8_t q = led_pos[id];
        if (q == 0xFF)
            continue;
        if (on)
            nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
        else if (dim)
            dl[q >> 3] |= (uint8_t)(1u << (q & 7u));
    }
    /* PLAY's second, green light: the matrix position with no key at column 8, row PA5 (found by
     * lighting the unused positions: 9 and 10 light nothing) */
    if (buttons & LED_PLAY_GREEN)
        nl[8] |= 1u << 1;
    else if (dim_buttons & LED_PLAY_GREEN)
        dl[8] |= 1u << 1;
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led[c] = nl[c];
        fm1_led_dim[c] = dl[c];
    }
    fm1_led_dim_div = DIV[glow < 0 || glow > 2 ? 0 : glow];
}

static void ble_poll(void);
void plat_yield(void) { ble_poll(); }          /* Bluetooth: its notifications go out while the screen draws */

/* MASTER: the pot, read in the main loop (main_fm1.c) */
static volatile uint32_t master_q12;
static volatile uint32_t audio_cpu_pct, audio_xruns;   /* the audio interrupt's (main_fm1.c) */
uint32_t plat_cpu_pct(void) { return audio_cpu_pct; }
uint32_t plat_master(void) { return master_q12; }

/* the 1 kHz tick (engine_tick) runs in the TIMER5 interrupt: the main loop locks it out around what
 * they share. The lock nests (the engine sends MIDI while holding it, and sending locks too);
 * MIDI sent from the tick itself needs no lock (nothing that sends interrupts it). */
static volatile uint8_t in_tick;
static uint32_t lock_depth;
uint32_t plat_lock(void)
{
    fm1_irq_off();
    return lock_depth++;
}
void plat_unlock(uint32_t f)
{
    (void)f;
    if (lock_depth && --lock_depth == 0)
        fm1_irq_on();
}

/* MIDI: Felucca's rings carry 4-byte USB-MIDI event packets, byte 0 = cable / CIN. USB and the TRS
 * jack share midi_in_q (both from TIMER5); the jack's packets carry cable 1 (midi_uart.c), which our
 * single-jack USB descriptor never does. Bluetooth has its own queue (ble.c, main loop). */
static int ble_midi_in(uint32_t *pkt);
static void ble_midi_tap(uint32_t pkt);
int plat_midi_in(uint32_t *pkt, int *port)
{
    if (mi_r != mi_w) {
        uint32_t p = midi_in_q[mi_r % MQ];
        RING_PUBLISH();
        mi_r++;
        *port = (p >> 4 & 0xFu) == 1u ? PORT_TRS : PORT_USB;
        *pkt = p & ~0xF0u;
        return 1;
    }
    if (ble_midi_in(pkt)) {
        *port = PORT_BLE;
        return 1;
    }
    return 0;
}
void plat_midi_send(uint32_t pkt, uint32_t to)   /* the main loop's and the tick's rings are shared */
{
    uint32_t f = 0;
    if (!in_tick)
        f = plat_lock();
    if (to & TO_USB)
        midi_out_event(pkt);
    if (to & TO_BLE)
        ble_midi_tap(pkt);
    if (((to & TO_SYNTH) || ((to & (TO_USB | TO_BLE)) && !(to & TO_NOSYNTH))) && proj.sound != SOUND_OFF) {
        uint32_t st = (pkt >> 8) & 0xFFu;      /* channel messages only: the synth's queue is single-writer, */
        if (st >= 0x80u && st < 0xF0u)         /* and this runs under the lock or in the tick */
            gm_midi(st, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
    }
    if (!in_tick)
        plat_unlock(f);
}

int plat_store_load(uint32_t obj, void *dst, uint32_t max) { return flash_ok ? st_load(obj, dst, max) : -1; }
int plat_store_save(uint32_t obj, const void *src, uint32_t len) { return flash_ok ? st_save(obj, src, len) : -9; }
