// SPDX-License-Identifier: GPL-3.0-only
// The browser build in Node: boot it, play a key and turn a knob, expect the MIDI out and screen frames,
// and the synth's sound: silence at rest, a note while a key is held, nothing with MASTER down.
import fs from "fs";
const wasm = fs.readFileSync(process.argv[2]);
let mem;
const wasi = new Proxy({ clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } }, { get: (t, k) => k in t ? t[k] : () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: wasi, env: new Proxy({}, { get: () => () => 0 }) });
const ex = instance.exports; mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_boot();
ex.web_master(3072);
const sent = [];
let peak = 0;
function run(quanta) {
  peak = 0;
  for (let i = 0; i < quanta; i++) {
    ex.web_render(128);
    for (const v of new Float32Array(mem.buffer, ex.web_out_l(), 128)) peak = Math.max(peak, Math.abs(v));
    const n = ex.web_out_n();
    if (n) { sent.push(...new Uint32Array(mem.buffer, ex.web_out_pkts(), n)); ex.web_out_clear(); }
  }
}
const hex = (p) => [(p >>> 8) & 0xFF, (p >>> 16) & 0xFF, (p >>> 24) & 0xFF].map((b) => b.toString(16).padStart(2, "0")).join(" ");
run(40);
const quiet = peak;
ex.web_keys(1 << 7); run(200); const sounding = peak; ex.web_keys(0); run(20);   // C4
ex.web_enc(3, 1); run(20);                                       // KNOB 1: CC 74 64 -> 65
ex.web_midi_in(2, (0x9 | 0x90 << 8 | 60 << 16 | 99 << 24) >>> 0); run(10);   // the TRS jack: thru to USB
// Touch on (Setup, SELECT, KNOB 1: Keys): a key that lands at 40 plays at about 40
ex.web_buttons(1 << 4); run(10); ex.web_buttons(0); run(10);   // EDIT
ex.web_enc(0, 1); run(10); ex.web_enc(3, 1); run(10);
const mark = sent.length;
ex.web_key(7, 40, 0); run(20); ex.web_key(7, 0, 0); run(20);
const touchVel = sent.slice(mark).map((p) => [(p >>> 8) & 0xFF, (p >>> 16) & 0xFF, (p >>> 24) & 0xFF])
  .filter(([s, n, v]) => s === 0x90 && n === 60 && v).map(([, , v]) => v)[0] ?? 0;
ex.web_enc(3, -1); run(10); ex.web_buttons(1 << 6); run(10); ex.web_buttons(0); run(10);   // Touch off; HOME
const want = ["90 3c 64", "80 3c 00", "b0 4a 41", "90 3c 63"];
const got = sent.map(hex);
ex.web_master(0); run(200); run(20);
const muted = peak;
console.log("sent", got.join(", "), "| blits", ex.web_blits(), "| peak at rest", quiet, "playing", sounding.toFixed(3), "MASTER down", muted, "| touch velocity", touchVel);
const ok = want.every((w) => got.includes(w)) && ex.web_blits() > 3 && quiet === 0 && sounding > 0.01 && muted === 0 && touchVel >= 35 && touchVel <= 45;
process.exit(ok ? 0 : 1);
