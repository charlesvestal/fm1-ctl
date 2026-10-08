// SPDX-License-Identifier: GPL-3.0-only
// The browser build in Node: boot it, play a key and turn a knob, expect the MIDI out and screen frames.
import fs from "fs";
const wasm = fs.readFileSync(process.argv[2]);
let mem;
const wasi = new Proxy({ clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; },
  fd_write: (fd, iov, n, out) => { new DataView(mem.buffer).setUint32(out, 0, true); return 0; } }, { get: (t, k) => k in t ? t[k] : () => 0 });
const { instance } = await WebAssembly.instantiate(wasm, { wasi_snapshot_preview1: wasi, env: new Proxy({}, { get: () => () => 0 }) });
const ex = instance.exports; mem = ex.memory;
if (ex._initialize) ex._initialize();
ex.web_boot();
const sent = [];
function run(quanta) {
  for (let i = 0; i < quanta; i++) {
    ex.web_render(128);
    const n = ex.web_out_n();
    if (n) { sent.push(...new Uint32Array(mem.buffer, ex.web_out_pkts(), n)); ex.web_out_clear(); }
  }
}
const hex = (p) => [(p >>> 8) & 0xFF, (p >>> 16) & 0xFF, (p >>> 24) & 0xFF].map((b) => b.toString(16).padStart(2, "0")).join(" ");
run(40);
ex.web_keys(1 << 7); run(20); ex.web_keys(0); run(20);          // C4
ex.web_enc(3, 1); run(20);                                       // KNOB 1: CC 74 64 -> 65
ex.web_midi_in(2, (0x9 | 0x90 << 8 | 60 << 16 | 99 << 24) >>> 0); run(10);   // the TRS jack: thru to USB
const want = ["90 3c 64", "80 3c 00", "b0 4a 41", "90 3c 63"];
const got = sent.map(hex);
console.log("sent", got.join(", "), "| blits", ex.web_blits());
const ok = want.every((w) => got.includes(w)) && ex.web_blits() > 3;
process.exit(ok ? 0 : 1);
