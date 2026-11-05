// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// The browser build as the page runs it (./build.sh --web): fm1-base's page with CTL-1's bits (tools/emu_page.py),
// its worklet with the emulator's parts: "resample" (in a browser whose audio runs at 48 kHz, many Macs, the FM-1's
// clock keeps the page's time: without it CTL-1 plays 48/44.1 fast and high), "midi" (what CTL-1 sends, to the
// page's MIDI out), and CTL-1's own (app_worklet.js: a key with its velocity, web_key). fm1-base's worklet under node
// (base/tests/emu_harness.mjs).
//   node tests/host/emu_page_test.mjs app/build/emu
import { existsSync, readFileSync } from "node:fs";
import { startEmu, check, done } from "../../base/tests/emu_harness.mjs";

const dir = process.argv[2] || "app/build/emu";
const read = (f) => existsSync(`${dir}/${f}`) ? readFileSync(`${dir}/${f}`, "utf8") : "";
const page = read("index.html");
check(page.includes("<title>CTL-1 in the browser</title>") && page.includes('<section class="help">') && page.includes('id="kvel"'),
      "fm1-base's page with CTL-1's name, help and keyboard velocity");
check(!page.includes("{{"), "no placeholder left in the page");
check(read("app_own.js").includes("CTL-1") && read("app.js").includes("app_own.js"), "CTL-1's app.js on the page");
check(read("app_worklet.js").includes("app_own_worklet.js"), "CTL-1's app_worklet.js in the worklet");
for (const p of ["resample", "midi"])
  check(read("app_worklet.js").includes(`${p}_worklet.js`) && existsSync(`${dir}/parts/${p}_worklet.js`), `the worklet has the part ${p}`);
const emu = await startEmu(dir, { sampleRate: 48000 });
emu.run(500);
const t0 = emu.ex.web_now();
emu.run(1000);
const ms = emu.ex.web_now() - t0;
check(Math.abs(ms - 1000) <= 12, `at 48 kHz the FM-1 ran ${ms} ms in the page's 1000`);
emu.take("midi");
emu.send({ type: "app", key: 7, vel: 40, press: 0 });                     // C4, as app.js sends it
const sounding = emu.run(300);
emu.send({ type: "app", key: 7, vel: 0, press: 0 });
emu.run(100);
const out = emu.take("midi").flatMap((m) => m.msgs).map((b) => [...b]);
check(out.some((b) => b[0] === 0x90 && b[1] === 60 && b[2]) && out.some((b) => (b[0] === 0x80 || (b[0] === 0x90 && !b[2])) && b[1] === 60),
      `a key from the page: note on and off to the MIDI out (${out.filter((b) => b[0] !== 0xf8).map((b) => b.map((x) => x.toString(16)).join(" ")).join(", ")})`);
check(sounding > 0.01, `and the FM-1's own sound plays it (peak ${sounding.toFixed(3)})`);
done();
