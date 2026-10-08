/* Bluetooth MIDI: JieLi's BT stack (btctrler.a, btstack.a, wl_rf_common.a: Apache-2.0, binary only;
 * the source may be shared, built firmware not until the licence is settled) on the shim in
 * ../../ble/shim. Started at every boot, as the stock firmware does, unless the last Bluetooth run
 * ended in a crash or a watchdog reset (then by hand, from the Bluetooth page). Part of the unity
 * build (main.c). A short log and a crash note survive a reset (the Bluetooth page shows them). */
#include "../../../ble/shim/shim.h"

/* ---- the platform the shim asks for */
uint32_t shim_ms(void) { return fm1_uptime_ms(); }   /* TIMER4: independent of the timer-5 tick */

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
void shim_request_irq(int index, int prio, void (*handler)(void))
{
    uint32_t f = irq_save();
    fm1_guard_unlock_top();
    fm1_irq_attach((uint32_t)index, handler, (uint32_t)(prio + 5 > 7 ? 7 : prio + 5));
    fm1_guard_lock_top();
    irq_restore(f);
}

/* a short log for the screen, kept through a reset (.noinit): after a crash the Bluetooth page shows
 * what the last run said, and a crash note */
#define BLE_LOG_N 12
#define BLE_LOG_MAGIC 0x424C4532u                 /* "BLE2" */
static struct {
    uint32_t magic, w, crash_seen, running, repeat, t0_up;   /* running: a BT run was on (cleared at boot) */
    char line[BLE_LOG_N][40];
} blog __attribute__((section(".noinit")));
void shim_wdt(void) { fm1_wdt_feed(); }
void shim_log(const char *s)
{
    char *d = blog.line[blog.w % BLE_LOG_N];
    uint32_t i;
    if (blog.magic != BLE_LOG_MAGIC)
        return;
    if (blog.w) {                                 /* the same start as the last line: fold it, "xN" */
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
            return;
        }
    }
    blog.repeat = 0;
    for (i = 0; i < 39u && s[i]; i++)
        d[i] = s[i] >= 32 && s[i] < 127 ? s[i] : ' ';
    d[i] = 0;
    blog.w++;
}
static void hex8(char *b, uint32_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15u];
}
static char *put_dec(char *b, uint32_t v)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

/* at boot: what ended the last run. The crash note: the fault's pc, vector and debug register, its
 * return address and how long Bluetooth had run, and the core's last four branches (ETM). */
static uint8_t ble_last_bad;                      /* the last BT run ended in a crash or a watchdog reset */
static void ble_boot_note(void)
{
    char b[40];
    int crashed = fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != blog.crash_seen;
    if (blog.magic != BLE_LOG_MAGIC || blog.w > 100000u) {
        if (!crashed)
            return;
        blog.magic = BLE_LOG_MAGIC;               /* a crash with Bluetooth never started: note it too */
        blog.w = blog.running = blog.t0_up = 0;
    }
    if (crashed) {
        char *q;
        blog.crash_seen = fm1_crash.count;
        q = put_dec(b + 6, fm1_crash.vec);
        b[0] = 'c'; b[1] = 'r'; b[2] = 'a'; b[3] = 's'; b[4] = 'h'; b[5] = ' ';
        q[0] = ' '; q[1] = 'p'; q[2] = 'c'; q[3] = ' ';
        hex8(q + 4, fm1_crash.pc);
        q[12] = ' '; q[13] = 'd'; q[14] = ' ';
        hex8(q + 15, fm1_crash.dbg);
        q[23] = 0;
        shim_log(b);
        b[0] = 'r'; b[1] = 'e'; b[2] = 't'; b[3] = ' ';
        hex8(b + 4, fm1_crash.rets);
        b[12] = ' '; b[13] = 'm'; b[14] = 's'; b[15] = ' ';
        put_dec(b + 16, blog.running ? fm1_crash.uptime_ms - blog.t0_up : 0u);
        shim_log(b);
        b[0] = 'e'; b[1] = ' ';
        hex8(b + 2, fm1_crash.etm[0]);
        b[10] = ' ';
        hex8(b + 11, fm1_crash.etm[1]);
        b[19] = ' ';
        hex8(b + 20, fm1_crash.etm[2]);
        b[28] = ' ';
        hex8(b + 29, fm1_crash.etm[3]);
        b[37] = 0;
        shim_log(b);
    } else if (blog.running && (fm1_boot.p3_rst & 4u)) {
        shim_log("reset by the watchdog");
    }
    ble_last_bad = (uint8_t)(blog.running && (crashed || (fm1_boot.p3_rst & 4u)));
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
    if (from == 3 && e && e->event == 3 && ble_state == BLE_STARTING) {   /* BT_EVENT_FROM_CON, BT_STATUS_INIT_OK */
        shim_log("BT ready: advertising as FM-1");
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
/* BLE_MASK_AT_START: mask the input timer while the radio calibrates. fm1-ble needed it while the RF
 * trim ran from the flash (the real cause of its crashes, notes/06); with the trim in RAM it should
 * not be, and without it keys, USB and the screen do not pause when Bluetooth starts. */
#ifndef BLE_MASK_AT_START
#define BLE_MASK_AT_START 0
#endif
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
    blog.w = 0;
    blog.crash_seen = fm1_crash.count;
    blog.running = 1;
    ble_t0 = blog.t0_up = fm1_uptime_ms();
    shim_log("BT: starting");
#if BLE_MASK_AT_START
    {
        uint32_t f = irq_save();
        fm1_irq_mask(FM1_IRQ_TIMER5);
        irq_restore(f);
        irqs_masked = 1;
    }
#endif
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
    {   /* ten seconds after it is ready, once: the tasks' deepest stack so far (sizing; 8 KiB each) */
        static uint8_t said;
        extern int shim_stack_depth(int task);
        if (ble_state == BLE_READY && !said && fm1_uptime_ms() - ble_t0 > 10000u) {
            char b[40] = "stacks ", *q = b + 7;
            int k;
            said = 1;
            for (k = 1; k <= 3; k++) {
                q = put_dec(q, (uint32_t)shim_stack_depth(k) * 256u);
                *q++ = k < 3 ? ' ' : 0;
            }
            q[-1] = 0;
            shim_log(b);
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
