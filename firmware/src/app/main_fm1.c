/* SPDX-License-Identifier: GPL-3.0-only
 * Boot, audio ISR and main loop adapted from Felucca's main.c / audio.c, Copyright (C) 2026
 * Leo Kuroshita (@kurogedelic), Hugelton Instruments. */
/* Ctl on the FM-1 (from FoMni's): boot (WDT first, boot-loop guard, guards), LCD, input (TIMER5
 * 10 kHz), USB, the TRS jack, audio (ALNK0: the onboard GM synth, below every other interrupt); then the
 * main loop: UI at ~60 frames/s, input and MIDI polled in between, Bluetooth's turn (ble_poll) when it runs. */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];

#define DBG_MAGIC 0x44424731u                       /* "DBG1": read with `fm1t memr` */
struct ct_dbg {
    uint32_t magic, timer_irqs, ui_frames, boots;
    uint32_t crash_seen;                            /* the crash count already reported at a boot */
} ct_dbg __attribute__((section(".noinit")));
static uint8_t safe_mode;                           /* two failed boots: USB on, nothing else (safe_main) */

/* --------------------------------------------------------------- audio --- */
/* the GM synth (dsp/gm.c) in Felucca's 256-frame halves (5.8 ms), at priority 3: below the 1 kHz tick
 * (4), the radio (5..7) and everything else, so it never delays them; it renders in what is left */
#define HALF_FRAMES 256u
#define HALF_WORDS (HALF_FRAMES * 2u)
#define FS 44100u
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));
static volatile uint32_t audio_cpu_q8;               /* audio_cpu_pct, audio_xruns: plat_fm1.c */
static volatile uint32_t audio_gain_q12;            /* MASTER, as volume */

static void render_block(int32_t *o)                /* ALNK0: one half, measured */
{
    uint32_t t0 = fm1_ticks(), us, budget = HALF_FRAMES * 1000000u / FS;
    if (proj.sound == SOUND_OFF || ble_quiet)
        gm_skip(o, HALF_FRAMES);                    /* off, or the radio calibrating (ble.c) */
    else
        gm_render(o, HALF_FRAMES, audio_gain_q12);
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    audio_cpu_q8 = (audio_cpu_q8 * 15u + (us * 256u) / budget) / 16u;
    audio_cpu_pct = (audio_cpu_q8 * 100u) >> 8;
}
void fm1_alnk0_irq(void)                            /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        render_block(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        if (fm1_audio_free_half() != half)
            audio_xruns++;                          /* the DMA moved on while we rendered */
    }
}
extern void isr_alnk0(void);
static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
static void audio_silence(void)                     /* IRQs off (flash erase): the DMA would loop stale audio */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}

/* ------------------------------------------------------------ timer --- */
void fm1_timer5_irq(void)
{
    static uint32_t sub;
    fm1_timer5_ack();
    ct_dbg.timer_irqs++;
    fm1_input_tick();
    if (sub % 5u == 0u) {                           /* 2 kHz: all USB SIE traffic, and the TRS jack */
        usb_poll();
        uart_midi_poll();
    }
    if (sub == 0u) {                                /* 1 kHz: the clock, arpeggiator and sequencer */
        in_tick = 1;
        engine_tick();
        in_tick = 0;
    }
    if (++sub == 10u)
        sub = 0;
    {
        static uint32_t last, acc;
        uint32_t now = fm1_ticks();
        acc += now - last;
        last = now;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
}
extern void isr_timer5(void);

/* ------------------------------------------------------------- boot --- */
#define BOOTGUARD_MAGIC 0x42475244u
struct bootguard_s bootguard __attribute__((section(".noinit")));   /* declared in main.c (ota_commit) */

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    fm1_audio_stop();
    char b[12];
    uint32_t t0;
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, "Ctl CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(10, 40, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->pc);
    draw_text_box(10, 60, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->emu);
    draw_text_box(10, 84, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->dbg);
    draw_text_box(10, 102, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->rets);
    draw_text_box(10, 120, 220, &FONT_S, b, C_WHITE, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 110, 240, &FONT_S, why, C_WHITE, 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                          /* intentional reset: not a failed boot */
    fm1_enter_uboot();
}

/* SAFE MODE: Ctl crashed (or hung) twice while starting. No UI, no Bluetooth; USB is on, so the web
 * installer (any firmware, or the stock one) and M-UPGRADE can reach it. PLAY tries Ctl again. The chip's own update mode stays the last resort (fm1_cstart). */
static void safe_main(void)
{
    char b[24];
    uint32_t t0, play;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_B, "Ctl SAFE MODE", C_HI, 1);
    draw_text_box(0, 52, 240, &FONT_S, "IT CRASHED TWICE", C_WHITE, 1);
    draw_text_box(0, 72, 240, &FONT_S, "WHILE STARTING.", C_WHITE, 1);
    draw_text_box(0, 102, 240, &FONT_S, "USB IS ON:", C_WHITE, 1);
    draw_text_box(0, 122, 240, &FONT_S, "REINSTALL FROM THE", C_WHITE, 1);
    draw_text_box(0, 142, 240, &FONT_S, "WEB INSTALLER.", C_WHITE, 1);
    draw_text_box(0, 172, 240, &FONT_S, "PLAY: TRY AGAIN", C_HI, 1);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 208, 240, &FONT_S, b, C_GRAY, 1);
    }
    fm1_input_init();
    panel_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 1);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    play = 1u << panel.btn[B_PLAY];
    t0 = fm1_ms;
    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (bootguard.pending && fm1_ms - t0 > 10000u)
            bootguard.pending = 0;                  /* safe mode itself is up: not another failed boot */
        if ((fm1_in.buttons & play) && fm1_ms - t0 > 500u) {
            bootguard.failed = 0;                   /* try Ctl again, from scratch */
            bootguard.pending = 0;
            draw_text_box(0, 172, 240, &FONT_S, "STARTING Ctl...", C_HI, 1);
            usb_detach();
            fm1_delay_ms(30);
            fm1_reboot();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();                      /* an install commits and resets; else back here */
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
    }
}

static void splash(void)
{
    lcd_fill(0, 0, 240, 240, RGB(250, 244, 232));
    cv_begin(240, 64, RGB(250, 244, 232));
    {
        int32_t w = text_w(&FONT_L, "Ctl");
        cv_text((240 - w) / 2, 0, &FONT_L, "Ctl", RGB(38, 166, 154));
        w = text_w(&FONT_S, "A MIDI controller for the FM-1");
        cv_text((240 - w) / 2, 42, &FONT_S, "A MIDI controller for the FM-1", RGB(150, 136, 118));
    }
    cv_blit(0, 92);
    lcd_sync();
}

static void fm1_main(void)
{
    uint32_t last_frame = 0;
    {
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;   /* the expected 1 MiB part, else RAM only */
        irq_restore(f);
        if (flash_ok)
            ota_boot_cleanup();
    }
    lcd_init();
    palette_set(1);
    splash();
    if (ct_dbg.magic != DBG_MAGIC) {
        memset(&ct_dbg, 0, sizeof ct_dbg);
        ct_dbg.magic = DBG_MAGIC;
    }
    if (safe_mode)
        safe_main();                                /* never returns */
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != ct_dbg.crash_seen) {
        char b[16];                                 /* a crash since the last boot: say so, a moment */
        ct_dbg.crash_seen = fm1_crash.count;
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 176, 240, &FONT_S, "RESTARTED AFTER A CRASH", RGB(255, 80, 60), 1);
        draw_text_box(0, 196, 240, &FONT_S, b, C_GRAY, 1);
        fm1_wdt_feed();
        fm1_delay_ms(2500);
        fm1_wdt_feed();
    }
    ct_dbg.boots++;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    led_pos_init();
    project_load();                                 /* defaults when nothing (or another format) is saved */
    gm_init();                                      /* the onboard GM synth, then its output */
    audio_init();
    usb_start();
    uart_midi_init();                               /* before TIMER5: it read-modify-writes port H too */
    fm1_timer5_start(isr_timer5, 4);                /* 4: below the radio's interrupts (5..7, ble.c) */
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & 3u) == 3u)
        panel_setup();                              /* OCT- + OCT+ held at power-on */
    fm1_delay_ms(400);
    ui_init();
    ble_boot_note();                                /* what ended the last Bluetooth run, into its log */
    if (!ble_last_bad)
        ble_start();                                /* Bluetooth is on, as on the stock firmware */
    else
        ui_say("BLUETOOTH OFF:", "IT CRASHED");     /* not twice in a row: KNOB 1 on its page starts it */

    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {   /* OCT- + OCT+ held 5 s: update mode (Felucca's, unchanged) */
            static uint32_t t0, shown;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both) {
                if (shown)
                    ui_say("UPDATE MODE", "CANCELLED");
                shown = 0;
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != shown) {
                    char d[4] = {(char)('0' + left), '.', '.', 0};
                    ui_say("UPDATE MODE IN", d);
                    shown = left;
                }
            } else if (fm1_ms - t0 > 5000u) {
                enter_uboot("UBOOT");
            }
        }
        ota_service();                              /* M-UPGRADE handshake (other SysEx is dropped) */
        if (usb.ota_req) {
            usb.ota_req = 0;
            gm_panic();
            if (flash_ok)
                ota_session();                      /* returns only if nothing was committed */
            ui_init();
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
#if CT_CDC
        cdc_task();
#endif
        ble_poll();                                 /* Bluetooth's tasks, once started */
        {   /* MASTER: smoothed, 0..4096 (the UI sends it as a controller when asked to) */
            static int32_t knob = -1;
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                knob = knob < 0 ? a * 16 : knob + (a * 16 - knob) / 8;
                master_q12 = (uint32_t)(knob / 4);
                audio_gain_q12 = (master_q12 * master_q12) >> 12;   /* volume: a squared curve */
            }
        }
        if (fm1_ms - last_frame >= 16u) {           /* ~60 frames/s at most; input in between */
            last_frame = fm1_ms;
            ct_dbg.ui_frames++;
            ui_frame();
        } else {
            ui_input_only();
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 4u) {                   /* safe mode failed too: the chip's own update mode */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    p3 |= (bootguard.failed >= 2u) << 8;            /* two failed boots: safe mode (kept past the .bss clear) */
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    safe_mode = (uint8_t)(p3 >> 8);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
