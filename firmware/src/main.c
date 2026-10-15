/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl firmware for the M-VAVE FM-1, a MIDI controller (USB, Bluetooth, the TRS jack in): one
 * compilation unit for the platform and the app (Felucca's layout, as X0X: the HAL is header-only,
 * so everything that touches it is here). JieLi's BT stack and the shim are linked beside it
 * (tools/build.py), except in the USB-only build (--no-ble, CT_BLE 0). -Os. Order matters. */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
/* the keys' velocity and aftertouch (MONK-1's, from Velo): the scan samples the analog rows (plat_fm1.c) */
#define FM1_INPUT_ANALOG(c, r, v, d) plat_analog_sample(c, r, v, d)
#define FM1_INPUT_DRIVE(c, r) plat_analog_drive(c, r)
#define FM1_INPUT_PROBE(c, r, v, p) plat_probe_sample(c, r, v, p)
#define FM1_INPUT_LED_OK(c, r) plat_analog_led_ok(c, r)
static void plat_analog_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven);
static int plat_analog_drive(uint32_t col, uint32_t rowbit);
static void plat_probe_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t pressed);
static int plat_analog_led_ok(uint32_t col, uint32_t rowbit);
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "fm1_flash.h"
#include "fm1_xip.h"
#include "fm1_perf.h"

#include "libc.c"
#include "lcd.c"
#include "gfx.c"

static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR) */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

#include "app/plat.h"

#define FELUCCA_OTA 1             /* M-UPGRADE update entry: keeps the web installer working */
#ifndef CT_CDC
#define CT_CDC 0                  /* USB serial console off: one plain MIDI interface */
#endif
#define FELUCCA_CDC CT_CDC
#define FELUCCA_UAC 0             /* no audio: no USB audio input */
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_600"     /* package identity (tools/build.py) */
#endif
#include "usb.c"
#include "midi_uart.c"            /* MIDI IN on the TRS jack */

static uint8_t flash_ok;          /* JEDEC id matched at boot */
static void audio_silence(void);   /* Felucca's hook before a flash erase (main_fm1.c) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: input keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static int st_erase(uint32_t off)
{
    uint32_t took;
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    audio_silence();
    return fl_erase4k(off, &took);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!FL_STORE_OK(off, n))
        return -8;
    return fl_write(off, src, n);
}
#include "storage.c"

struct bootguard_s { uint32_t magic, failed, pending; };
extern struct bootguard_s bootguard;
#include "ota.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_wdt_feed(); }
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    audio_silence();
    return fl_erase4k(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 92, 240, &FONT_S, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 124, 240, &FONT_S, STEP[step < 6u ? step : 0], C_HI, 1);
        return;
    }
    draw_text_box(0, 124, 240, &FONT_S, code == 1 ? "DRY RUN OK" : "FAILED", C_HI, 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard.pending = 0;                              /* intentional reset */
    bootguard.failed = 0;                               /* a new install starts with a clean slate */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}

#include "app/app.h"
#define ANN_NAME "CTL-1 " CT_VERSION
#define ANN_DATA __attribute__((section(".pool")))
#include "ann_words.h"            /* the link to CTL-1's words file (tools/gen_words.py, from firmware/words.tsv) */
#include "announce.c"             /* the screen as text for a screen reader (fm1-reader), over USB only */
static int ann_wire_send(const uint8_t *p, uint32_t n)   /* never waits: no room (nobody reading), not sent */
{
    if (!usb.config || usb.suspended || SXQ - (so_w - so_r) < (n + 2u) / 3u)
        return -1;
    return ota_wire_send(p, n);
}
#include "app/sense.c"
#include "app/panel.c"
#include "app/plat_fm1.c"
#include "app/project.c"
#include "app/engine.c"
#include "app/ui.c"
#if CT_BLE
#include "app/ble.c"
#else
#include "app/ble_none.c"              /* the USB-only build (--no-ble) */
#endif
#include "app/main_fm1.c"
