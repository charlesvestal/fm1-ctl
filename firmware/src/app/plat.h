/* SPDX-License-Identifier: GPL-3.0-only */
/* Ctl platform interface (from FoMni's / X0X's): everything the app needs from the hardware.
 *
 * The device implementation (plat_fm1.c, ble.c) wraps Felucca's HAL, the panel map, USB, the TRS
 * jack and the BLE stack; the host implementation (host/ctl_host.c) is a simulator fed by a script,
 * which is what lets the whole app run and be tested on a Mac. Drawing goes through Felucca's
 * lcd_fill / lcd_blit (lcd.c on the device, a framebuffer on the host) and gfx.c on top. */
#pragma once
#include <stdint.h>

/* panel controls, by printed label (Felucca's panel.c map turns these into matrix ids; Felucca
 * calls the second button SCL, the panel prints SEL) */
enum { B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC,
       B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };

#define NKEYS 27                       /* F3 .. G5 */

uint32_t plat_ms(void);
uint32_t plat_buttons(void);           /* held: bit B_* */
uint32_t plat_keys(void);              /* held: bit k = key k (0 = F3) */
int32_t plat_enc(int role);            /* detents since the last call, + = clockwise; EN_* */
void plat_leds(uint32_t buttons, uint32_t keys);   /* lit: bit B_*, bit key */

/* MIDI: 4-byte USB-MIDI event packets (byte 0 = cable / CIN, then status, data 1, data 2). The TRS
 * jack is MIDI IN only (the FM-1 has no MIDI out jack). Main loop only. */
enum { PORT_USB, PORT_BLE, PORT_TRS, NPORTS };
#define TO_USB (1u << PORT_USB)
#define TO_BLE (1u << PORT_BLE)
int plat_midi_in(uint32_t *pkt, int *port);   /* 1 = got one, from port PORT_* */
void plat_midi_send(uint32_t pkt, uint32_t to);   /* to: TO_USB | TO_BLE */

/* Bluetooth (BLE MIDI): off at boot, started by hand; once started it runs until a restart */
enum { BLE_OFF, BLE_STARTING, BLE_READY, BLE_FAILED };
int plat_ble_state(void);
int plat_ble_connected(void);          /* a central is connected and listening */
void plat_ble_start(void);
uint32_t plat_ble_log_total(void);     /* lines written since the last start (survives a reset) */
const char *plat_ble_log_line(int back);   /* 0 = the newest */

/* persistent storage (Felucca storage.c objects: A/B sectors, CRC, torn-write safe) */
int plat_store_load(uint32_t obj, void *dst, uint32_t max);   /* bytes loaded, < 0 = none / bad */
int plat_store_save(uint32_t obj, const void *src, uint32_t len);   /* 0 = ok */
#define PLAT_STORE_MAX 3840u           /* bytes per object */
enum { OBJ_PROJ, OBJ_PANEL, OBJ_BLE, OBJ_NOBJ };
/* flash: OBJ_PROJ 0xFC000, OBJ_PANEL 0xDC000, OBJ_BLE 0xDE000 (2 sectors each, in Felucca's data area) */
