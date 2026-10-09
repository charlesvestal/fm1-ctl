/* SPDX-License-Identifier: GPL-3.0-only */
/* The USB-only build (tools/build.py --no-ble, CT_BLE 0): what ble.c gives the rest of the firmware,
 * with no Bluetooth behind it. Nothing from JieLi's BT libraries or the shim is linked, so this build
 * can be distributed under the GPL as it is. Part of the unity build (main.c). */
static const uint8_t ble_quiet = 0;               /* the synth never holds for the radio */
static const uint8_t ble_last_bad = 0;
static void ble_boot_note(void) {}
static void ble_start(void) {}
static void ble_poll(void) {}
static int ble_midi_in(uint32_t *pkt) { (void)pkt; return 0; }
static void ble_midi_tap(uint32_t pkt) { (void)pkt; }

/* ---- plat.h */
int plat_ble_state(void) { return BLE_OFF; }
int plat_ble_connected(void) { return 0; }
void plat_ble_start(void) {}
const char *plat_ble_log_line(int back) { (void)back; return ""; }
uint32_t plat_ble_log_total(void) { return 0; }
