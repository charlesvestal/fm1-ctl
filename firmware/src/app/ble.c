/* Bluetooth MIDI: JieLi's BT stack (btctrler.a, btstack.a, wl_rf_common.a: Apache-2.0, binary only;
 * private until the licence is settled) on the shim in ../../ble/shim, from fm1-ble. Started by hand
 * (the Bluetooth page), never at boot: a crash restarts the FM-1 without Bluetooth. Part of the
 * unity build (main.c). The diagnostics (the log, the crash note, the live counters, trace.S) stay
 * while it is new. */
#include "../../../ble/shim/shim.h"

/* ---- the platform the shim asks for */
uint32_t shim_ms(void) { return fm1_uptime_ms(); }   /* TIMER4, not the timer-5 tick (masked while BT starts) */

static volatile uint32_t shim_depth;
uint32_t shim_irq_save(void)                     /* nests, unlike irq_save(): the shim calls itself */
{
    fm1_irq_off();
    return shim_depth++;
}
void shim_irq_restore(uint32_t f)
{
    (void)f;
    if (shim_depth && --shim_depth == 0)
        fm1_irq_on();
}
int shim_in_irq(void) { return 0; }
/* every task waits: run what is due (a timer may be what they wait for) and keep the watchdog fed,
 * whichever context we are in (btstack_init waits in the main loop's) */
static void ble_stuck_check(void);
void shim_idle(void)
{
    static uint8_t in;
    fm1_wdt_feed();
    if (!in) {
        in = 1;
        shim_timers();
        ble_stuck_check();
        in = 0;
    }
}
void shim_reboot(void) { fm1_reboot(); }
/* the shim's delays (JieLi's delay_us and friends): from RAM, timed by TIMER4, nothing called */
__attribute__((section(".ram_text"), noinline, used)) void shim_delay_us(uint32_t us) { fm1_busy_us_inl(us); }


/* the radio's interrupts sit above the input timer (4): JieLi asks for 0..2, mapped to 5..7 */
static uint8_t ble_irqs[8], ble_nirq;
/* diagnostics (shim/trace.S): every entry into JieLi's two ISRs is noted here, count at +0, their
 * handlers at +4 and +8, a ring of 16 {reti, sp - 32} at +32; kept through a reset for the crash note */
struct { uint32_t n, handler[2], rsv[5], ring[16][2]; } shim_isr_trace __attribute__((section(".noinit")));
extern void shim_tramp_event(void), shim_tramp_rx(void);
volatile uint32_t shim_delay_from __attribute__((section(".noinit"))), shim_delay_n __attribute__((section(".noinit")));
void shim_request_irq(int index, int prio, void (*handler)(void))
{
    uint32_t f = irq_save();
    if (index == 45) {
        shim_isr_trace.handler[0] = (uint32_t)(uintptr_t)handler;
        handler = shim_tramp_event;
    } else if (index == 29) {
        shim_isr_trace.handler[1] = (uint32_t)(uintptr_t)handler;
        handler = shim_tramp_rx;
    }
    fm1_guard_unlock_top();
    fm1_irq_attach((uint32_t)index, handler, (uint32_t)(prio + 5 > 7 ? 7 : prio + 5));
    fm1_guard_lock_top();
    irq_restore(f);
    if (ble_nirq < sizeof ble_irqs)
        ble_irqs[ble_nirq++] = (uint8_t)index;
}

/* a short log for the screen, kept through a reset (.noinit): after a crash or a watchdog reset the
 * Setup page shows what the last run said */
#define BLE_LOG_N 12
#define BLE_LOG_MAGIC 0x424C4531u                 /* "BLE1" */
static struct {
    uint32_t magic, w, count, crash_seen, running, repeat, probe;   /* probe: nmi_probe(7) at the last start */
    uint32_t clk0[4], clkl[4], t0_up;
    uint32_t pnd_seen[8];                         /* IPND bits seen during the run (shim_wdt) */
    uint8_t ana[10];                              /* P33 ANA_CON0..9 at the start: VDDIO, VDC14 (6, bits 0-2), SYSVDD (9, bits 0-3) */             /* t0_up: uptime_ms (TIMER4) at the start */                    /* clocks (fm1_clk_snapshot) at the start and at the last log line */
    uint8_t depth[4];                             /* each task's deepest stack, 256-byte units (the last seen) */
    char line[BLE_LOG_N][40];
} blog __attribute__((section(".noinit")));
#define ble_log_w blog.w
#define ble_log_count blog.count
void shim_wdt(void)                               /* also: which interrupts are pending while the library spins */
{
    fm1_wdt_feed();
    if (blog.magic == BLE_LOG_MAGIC)
        fm1_ipnd_or(blog.pnd_seen);
}
void shim_log(const char *s)
{
    char *d = blog.line[blog.w % BLE_LOG_N];
    uint32_t i;
    if (blog.magic != BLE_LOG_MAGIC)
        return;
    fm1_clk_snapshot(blog.clkl);
    if (blog.w) {                                 /* the same start as the last line: fold it, "(xN)" */
        char *p = blog.line[(blog.w - 1u) % BLE_LOG_N];
        for (i = 0; i < 5u && s[i] && s[i] == p[i]; i++)
            ;
        if (i == 5u || (!s[i] && !p[i])) {
            uint32_t n = ++blog.repeat + 1u, k = 0;
            char num[12];
            for (i = 0; i < 26u && s[i]; i++)
                p[i] = s[i] >= 32 && s[i] < 127 ? s[i] : ' ';
            p[i++] = ' ';
            p[i++] = 'x';
            do {
                num[k++] = (char)('0' + n % 10u);
                n /= 10u;
            } while (n && k < 10u);
            while (k)
                p[i++] = num[--k];
            p[i] = 0;
            blog.count++;
            return;
        }
    }
    blog.repeat = 0;
    for (i = 0; i < 39u && s[i]; i++)
        d[i] = s[i] >= 32 && s[i] < 127 ? s[i] : ' ';
    d[i] = 0;
    blog.w++;
    blog.count++;
}
static void hex8(char *b, uint32_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15u];
}
/* probe: is an interrupt of priority `prio` taken while cli holds? Soft interrupt 0 is raised with
 * interrupts off, then on. Result: bit 0 = it fired under cli, bit 1 = it fired after sti. */
static volatile uint32_t soft_hits;
void fm1_soft0_irq(void) { fm1_irq_soft_clr(0); soft_hits++; }
extern void isr_soft0(void);
static uint32_t nmi_probe(uint32_t prio)
{
    uint32_t f = irq_save(), a, b;
    fm1_guard_unlock_top();
    fm1_irq_attach(FM1_IRQ_SOFT0, isr_soft0, prio);
    fm1_guard_lock_top();
    soft_hits = 0;
    fm1_irq_soft_set(0);
    fm1_delay_us(20);
    a = soft_hits;
    irq_restore(f);
    fm1_delay_us(20);
    b = soft_hits;
    f = irq_save();
    fm1_irq_mask(FM1_IRQ_SOFT0);
    fm1_irq_soft_clr(0);
    irq_restore(f);
    return (a ? 1u : 0u) | (b ? 2u : 0u);
}
/* "ipm _ p7 _ p6 _ icfg ________": IPMASK, the probe at 7 and 6, icfg as the main loop sees it */
static void ble_env_line(char *o)
{
    const char *t = "ipm _ p7 _ p6 _ icfg ________";
    uint32_t i;
    for (i = 0; t[i]; i++)
        o[i] = t[i];
    o[i] = 0;
    o[4] = (char)('0' + (fm1_ipmask() & 15u));
    blog.probe = nmi_probe(7);
    fm1_clk_snapshot(blog.clk0);
    {
        uint32_t k;
        for (k = 0; k < 10u; k++)
            blog.ana[k] = fm1_p33_read(k);
    }
    o[9] = (char)('0' + blog.probe);
    o[14] = (char)('0' + nmi_probe(6));
    hex8(o + 21, fm1_icfg());
}

/* at boot: what ended the last BLE run (seven lines fit the screen; no two may share their first
 * five characters, or shim_log folds them) */
static void hex8s(char *o, const char *t, const uint32_t *v, int n)   /* t with n "________" slots */
{
    int i, k = 0;
    for (i = 0; t[i]; i++)
        o[i] = t[i];
    o[i] = 0;
    for (i = 0; t[i] && k < n; i++)
        if (t[i] == '_' && t[i + 1] == '_') {
            hex8(o + i, v[k++]);
            i += 7;
        }
}
static void ble_boot_note(void)
{
    char b[40];
    uint32_t v[4];
    if (fm1_crash.magic == FM1_CRASH_MAGIC && (blog.magic != BLE_LOG_MAGIC || blog.w > 100000u)) {
        blog.magic = BLE_LOG_MAGIC;               /* a crash with Bluetooth never started: note it too */
        blog.w = blog.count = blog.running = blog.probe = blog.t0_up = 0;
        blog.crash_seen = fm1_crash.count - 1u;
        memset(blog.ana, 0, sizeof blog.ana);
        memset(blog.pnd_seen, 0, sizeof blog.pnd_seen);
    }
    if (blog.magic != BLE_LOG_MAGIC || blog.w > 100000u)
        return;
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != blog.crash_seen) {
        uint32_t n = shim_isr_trace.n, *e = shim_isr_trace.ring[n & 15u];
        blog.crash_seen = fm1_crash.count;
        v[0] = fm1_crash.pc;
        v[1] = fm1_crash.dbg;
        hex8s(b, "pc ________ v## dbg ________ p7#", v, 2);        /* p7: the probe at the start, 1|3 = fires under cli */
        b[13] = (char)('0' + fm1_crash.vec / 10 % 10);
        b[14] = (char)('0' + fm1_crash.vec % 10);
        b[31] = (char)('0' + (blog.probe & 7u));
        shim_log(b);
        hex8s(b, "pnd ________ ________ ________", fm1_crash.ipnd, 3);              /* IRQ 0..31, 32..63, 96..127 */
        shim_log(b);
        {   /* P33 ANA_CON0..9 at the start (the SDK's BLE board: CON6 low bits 3 = VDC14 1.40 V, CON9 low bits 11 = SYSVDD 1.26 V) */
            char a[40] = "ana ";
            uint32_t k;
            for (k = 0; k < 10u; k++) {
                a[4 + 3 * k] = "0123456789ABCDEF"[blog.ana[k] >> 4];
                a[5 + 3 * k] = "0123456789ABCDEF"[blog.ana[k] & 15u];
                a[6 + 3 * k] = ' ';
            }
            a[4 + 3 * 10 - 1] = 0;
            shim_log(a);
        }
        v[0] = shim_delay_from;                                                     /* the last delay() call site, how many, ms into the run */
        v[1] = shim_delay_n;
        v[2] = fm1_crash.uptime_ms - blog.t0_up;
        hex8s(b, "dly ________ n ________ ms ________", v, 3);
        shim_log(b);
        hex8s(b, "seen ________ ________ ________ ________", blog.pnd_seen, 4);   /* pending bits seen, IRQ 0..127 */
        shim_log(b);
        hex8s(b, "e01 ________ ________ bt ________", fm1_crash.etm, 2);
        hex8(b + 25, n);
        shim_log(b);
        v[0] = fm1_crash.etm[2];
        v[1] = fm1_crash.etm[3];
        v[2] = e[0];
        hex8s(b, "e23 ________ ________ ri ________", v, 3);
        shim_log(b);
    } else if (blog.running) {
        shim_log("reset, no crash: watchdog?");
    }
    blog.running = 0;
}

int shim_store_load(void *dst, uint32_t max) { return plat_store_load(OBJ_BLE, dst, max); }
int shim_store_save(const void *src, uint32_t len) { return plat_store_save(OBJ_BLE, src, len); }

static volatile uint8_t ble_state;

/* ---- MIDI both ways (ble/midi/le_midi.c); everything here runs in the main loop */
extern int ble_midi_send_many(const uint32_t *pkts, const uint16_t *ms, int n);
extern int ble_midi_connected(void);
static uint32_t ble_rx_count, ble_tx_count;
static uint8_t ble_conn;                          /* ble_midi_connected(), as of the last poll */
#define BQ 64u
static uint32_t ble_in_q[BQ];
static uint32_t bi_w, bi_r;
void ble_midi_receive(uint32_t pkt)               /* from the stack (in ble_poll): for plat_midi_in */
{
    if (bi_w - bi_r < BQ) {
        ble_in_q[bi_w % BQ] = pkt & ~0xF0u;
        bi_w++;
        ble_rx_count++;
    }
}
static int ble_midi_in(uint32_t *pkt)
{
    if (bi_r == bi_w)
        return 0;
    *pkt = ble_in_q[bi_r % BQ];
    bi_r++;
    return 1;
}
/* out: every message with the millisecond it was queued (fm1_ms), which goes with it as its BLE MIDI
 * timestamp. MIDI clock waits in its own short ring (a few ticks at most: later than that they are no
 * use) so a stream of clock never holds up notes; the drain merges the two in time order. */
static uint32_t ble_out_q[BQ];
static uint16_t ble_out_ms[BQ];
static uint32_t bo_w, bo_r;
#define BCQ 4u
static uint16_t ble_clk_ms[BCQ];
static volatile uint32_t bc_w, bc_r;
static void ble_midi_tap(uint32_t pkt)            /* main loop (locked) or the tick; nothing queues for a central not there */
{
    uint16_t ms = (uint16_t)(fm1_ms & 0x1FFFu);
    if (ble_state != BLE_READY || !ble_conn)
        return;
    if (((pkt >> 8) & 0xFFu) == 0xF8u) {
        if (bc_w - bc_r < BCQ) {
            ble_clk_ms[bc_w % BCQ] = ms;
            RING_PUBLISH();
            bc_w++;
        }
        return;
    }
    if (bo_w - bo_r < BQ) {
        ble_out_q[bo_w % BQ] = pkt;
        ble_out_ms[bo_w % BQ] = ms;
        RING_PUBLISH();
        bo_w++;
    }
}
static int ms_before(uint16_t a, uint16_t b) { return ((uint32_t)(b - a) & 0x1FFFu) < 4096u && a != b; }
static void ble_midi_drain(void)                  /* main loop: what waits, as many notifications as the stack takes */
{
    int round;
    for (round = 0; round < 4; round++) {
        uint32_t pkts[16], n = 0, ni = 0, ci, nw, cw, sent, k;
        uint16_t ms[16];
        uint8_t is_clk[16];
        uint32_t f = plat_lock();                 /* a snapshot: the tick may add meanwhile */
        nw = bo_w;
        cw = bc_w;
        plat_unlock(f);
        ci = bc_r;
        ni = bo_r;
        while (n < 16u && (ni != nw || ci != cw)) {
            int take_clk = ci != cw && (ni == nw || ms_before(ble_clk_ms[ci % BCQ], ble_out_ms[ni % BQ]));
            if (take_clk) {
                pkts[n] = 0xFu | 0xF8u << 8;
                ms[n] = ble_clk_ms[ci % BCQ];
                is_clk[n++] = 1;
                ci++;
            } else {
                pkts[n] = ble_out_q[ni % BQ];
                ms[n] = ble_out_ms[ni % BQ];
                is_clk[n++] = 0;
                ni++;
            }
        }
        if (!n)
            return;
        sent = (uint32_t)ble_midi_send_many(pkts, ms, (int)n);
        if (!sent)
            return;                               /* the stack is full: what did not go stays for the next poll */
        for (k = 0; k < sent; k++) {
            if (is_clk[k])
                bc_r++;
            else
                bo_r++;
        }
        RING_PUBLISH();
        ble_tx_count += sent;
    }
}

/* ---- the stack */
static volatile uint32_t ble_events;
extern void btstack_init(void);
extern void bt_ble_init(void);
extern void lmp_set_sniff_disable(void);
extern void bt_max_pwr_set(uint8_t pwr, uint8_t pg_pwr, uint8_t iq_pwr, uint8_t ble_pwr);
#define BT_TX_POWER 8                             /* the SDK's default (classic, unused) */
#define BLE_TX_POWER 11                           /* the top: -64 dBm at 6 where the stock firmware gives -39 */
extern void lib_make_ble_address(uint8_t *ble, uint8_t *edr);
extern int le_controller_set_mac(void *addr);
extern int syscfg_read(uint16_t id, void *buf, uint16_t len);
extern int syscfg_write(uint16_t id, void *buf, uint16_t len);

struct bt_event_s { uint8_t event; uint8_t args[7]; uint32_t value; };
void shim_bt_event(int from, const void *event, int len)
{
    const struct bt_event_s *e = event;
    (void)len;
    ble_events++;
    {
        char b[40] = "event from ";
        b[11] = (char)('0' + (from & 7));
        b[12] = ' ';
        b[13] = 'e';
        b[14] = (char)('0' + (e ? e->event / 10 % 10 : 0));
        b[15] = (char)('0' + (e ? e->event % 10 : 0));
        b[16] = 0;
        shim_log(b);
    }
    if (from == 3 && e && e->event == 3 && ble_state == BLE_STARTING) {   /* BT_EVENT_FROM_CON, BT_STATUS_INIT_OK */
        shim_log("BT init ok: starting BLE");
        bt_ble_init();
        ble_state = BLE_READY;
    }
}

/* the BT address: the one kept, or one made from the chip's unique data (the SDK's own fallback is
 * a random one, kept) */
#define CFG_BT_MAC_ADDR 1                         /* include_lib/utils/syscfg/syscfg_id.h */
const uint8_t *bt_get_mac_addr(void)
{
    static uint8_t mac[6];
    static const uint8_t none[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t i, h = 2166136261u;
    if (syscfg_read(CFG_BT_MAC_ADDR, mac, 6) == 6 && memcmp(mac, none, 6))
        return mac;
    h ^= fm1_ticks();
    for (i = 0; i < 6u; i++) {
        h = (h ^ (h >> 13)) * 16777619u + fm1_ticks();
        mac[i] = (uint8_t)(h >> 8);
    }
    mac[0] = (uint8_t)((mac[0] & 0xFC) | 0xC0);   /* a static random address */
    syscfg_write(CFG_BT_MAC_ADDR, mac, 6);
    return mac;
}

static uint32_t ble_t0;
static uint8_t irqs_masked;
static void ble_irqs_back(const char *why)
{
    uint32_t f;
    if (!irqs_masked)
        return;
    irqs_masked = 0;
    f = irq_save();
    fm1_irq_unmask(FM1_IRQ_TIMER5);
    irq_restore(f);
    shim_log(why);
}
extern void shim_os_dump(void);
static void ble_stuck_check(void)                 /* still starting after 3 s: say what each task waits for */
{
    static uint32_t said;
    if (ble_state == BLE_STARTING && fm1_uptime_ms() - ble_t0 > 3000u && said != ble_t0) {
        said = ble_t0;
        shim_log("still starting after 3 s:");
        shim_os_dump();
    }
}

static void ble_start(void)
{
    uint8_t addr[6];
    if (ble_state != BLE_OFF)
        return;
    ble_state = BLE_STARTING;
    if (fm1_crash.magic != FM1_CRASH_MAGIC)
        fm1_crash.count = 0;
    blog.magic = BLE_LOG_MAGIC;
    blog.w = blog.count = 0;
    blog.crash_seen = fm1_crash.count;
    blog.running = 1;
    ble_t0 = fm1_uptime_ms();
    shim_log("BT: starting");
    {
        char e[40];
        ble_env_line(e);
        shim_log(e);
    }
    shim_isr_trace.n = 0;
    shim_delay_n = 0;
    memset(blog.pnd_seen, 0, sizeof blog.pnd_seen);
    shim_delay_from = 0;
    blog.t0_up = fm1_uptime_ms();
    {   /* no input-timer interrupt while the radio calibrates (ble_poll gives it back): every crash in
         * fm1-ble was an interrupt vectoring wrong during the calibration (notes/05). Keys, USB and
         * the screen pause for about a second. */
        uint32_t f = irq_save();
        fm1_irq_mask(FM1_IRQ_TIMER5);
        irq_restore(f);
        irqs_masked = 1;
        shim_log("timer irq masked");
    }
    shim_os_init();
    fm1_guard_stack_floor(shim_stacks_lo());      /* the tasks' stacks are in the pool */
    lmp_set_sniff_disable();
    /* the radio's transmit power, 0..11: the SDK sets it in its config start-up (apps/common/config/
     * user_cfg.c, cfg_file_parse: classic 8, BLE 6), which the shim does not run; left alone the
     * library transmits at its lowest (measured: -79 dBm where the stock firmware gives -39) */
    bt_max_pwr_set(BT_TX_POWER, 6, 6, BLE_TX_POWER);
    lib_make_ble_address(addr, (uint8_t *)bt_get_mac_addr());
    le_controller_set_mac(addr);
    btstack_init();
}

/* the main loop's turn: timers, then every BT task that can run */
static void ble_poll(void)
{
    if (ble_state == BLE_OFF)
        return;
    shim_timers();
    shim_run();
    ble_conn = (uint8_t)(ble_state == BLE_READY && ble_midi_connected());
    if (!ble_conn) {
        bo_r = bo_w;                              /* a central gone: nothing stale for the next one */
        bc_r = bc_w;
    }
    ble_midi_drain();
    ble_stuck_check();
    if (ble_state == BLE_READY)
        ble_irqs_back("irqs back: BT ready");
    else if (fm1_uptime_ms() - ble_t0 > 15000u)
        ble_irqs_back("irqs back: 15 s, not ready");
    {   /* every two seconds while running: BLE interrupts taken, stack events seen */
        static uint32_t t2;
        if (ble_state == BLE_READY && fm1_uptime_ms() - t2 > 2000u) {
            char b[40] = "irq ________ c_ rx ________ tx ________";
            t2 = fm1_uptime_ms();
            hex8(b + 4, shim_isr_trace.n);
            b[14] = (char)('0' + ble_midi_connected());
            hex8(b + 19, ble_rx_count);
            hex8(b + 31, ble_tx_count);
            shim_log(b);
        }
    }
    {   /* every half second: the tasks' deepest stack, kept for after a crash */
        static uint32_t t;
        extern int shim_stack_depth(int task);
        if (fm1_ms - t > 500u) {
            int k;
            t = fm1_ms;
            for (k = 0; k < 3; k++)
                blog.depth[k] = (uint8_t)shim_stack_depth(k + 1);
        }
    }
}

/* ---- plat.h */
int plat_ble_state(void) { return ble_state; }
int plat_ble_connected(void) { return ble_conn; }
void plat_ble_start(void) { ble_start(); }
const char *plat_ble_log_line(int back)
{
    if (blog.magic != BLE_LOG_MAGIC || back < 0 || (uint32_t)back >= blog.w || back >= BLE_LOG_N)
        return "";
    return blog.line[(blog.w - 1u - (uint32_t)back) % BLE_LOG_N];
}
uint32_t plat_ble_log_total(void) { return blog.magic == BLE_LOG_MAGIC && blog.w <= 100000u ? blog.w : 0u; }
