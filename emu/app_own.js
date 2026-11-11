// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// CTL-1's own part of fm1-base's emulator page (base/web/emu/index.html; fm1-base's build loads it as app_own.js
// beside the emulator's parts, when the emulator is switched on). It works through the page's own names (send,
// KEYMAP, holdDown, turn, knobs, buttons, keys, master, shortcuts, controls, node):
//   - the keys as the firmware senses them: the velocity a key lands with (lower on the key is harder; from the
//     computer's keyboard, the Keyboard velocity slider) and how hard it is pressed after (dragging down, a pen's
//     pressure), to the worklet as { type: "app", key, vel, press } (app_worklet.js: web_key) instead of the page's
//     key mask. They count with Touch on (Setup);
//   - the computer's keys as a piano (A the lowest key, W S E D ... up to '), and CTL-1's buttons on them;
//   - the keys named by their notes, Left and Right moving between them; what each control does, on hover;
//   - MASTER three-quarters up at the start, so it sounds at once.
// The MIDI ports are fm1-base's part "midi" (fm1.toml emu).

const ANALOG = 20;                       // keys 0..19 sense pressure: the top seven have no converter on the FM-1
const clamp01 = (x) => Math.max(0, Math.min(1, x));
const kvel = () => +document.getElementById("kvel").value;

// ------------------------------------------------------------------ the keys' velocity and pressure
const landing = [];                      // key -> the velocity its next press lands with (a pointer's; else the slider)
const pointers = new Map();              // pointer id -> { key, vel, press, y0, h }
const sent = [];                         // key -> { vel, press } as the worklet has it
function post(m) { if (node) node.port.postMessage({ ...m, type: "app" }); }
function keyTo(i, vel, press) {
  press = vel && i < ANALOG ? Math.round(press * 1000) : 0;
  const was = sent[i] || { vel: 0, press: 0 };
  if (!!vel === !!was.vel && (!vel || press === was.press)) return;
  sent[i] = { vel, press };
  post({ key: i, vel, press });
}
function senseKeys() {
  // the page's key mask (holding, latching, sliding, the keyboard: the page's own) becomes keys with a velocity
  let mask = 0;
  const pageSend = send;
  window.send = (m) => {
    if (!m || m.type !== "keys") return pageSend(m);
    const now = m.mask >>> 0, ch = now ^ mask;
    mask = now;
    for (let i = 0; i < keys.length; i++) {
      if (!(ch >> i & 1)) continue;
      if (now >> i & 1) {
        const p = [...pointers.values()].find((x) => x.key === i);
        keyTo(i, landing[i] || (p && p.vel) || kvel(), 0);
      } else {
        keyTo(i, 0, 0);
      }
      landing[i] = 0;
    }
  };
  // where a key is pressed is how hard it lands (lower is harder); dragging down from there presses harder
  controls.addEventListener("pointerdown", (e) => {
    const i = keys.findIndex((k) => k.hit === e.target);
    if (i < 0) return;
    const r = e.target.getBoundingClientRect();
    const vel = Math.max(1, Math.min(127, Math.round(1 + 126 * clamp01(((e.clientY - r.top) / r.height - 0.1) / 0.8))));
    landing[i] = vel;
    pointers.set(e.pointerId, { key: i, vel, press: 0, y0: e.clientY, h: r.height });
  }, true);
  controls.addEventListener("pointermove", (e) => {
    const p = pointers.get(e.pointerId);
    if (!p) return;
    let press = clamp01((e.clientY - p.y0) / (p.h * 0.6));
    if (e.pointerType === "pen") press = Math.max(press, clamp01((e.pressure - 0.3) / 0.6));
    if (Math.abs(press - p.press) < 0.01 && (press === 0) === (p.press === 0)) return;
    p.press = press;
    if (sent[p.key] && sent[p.key].vel) keyTo(p.key, sent[p.key].vel, press);
  }, true);
  const up = (e) => pointers.delete(e.pointerId);
  for (const t of ["pointerup", "pointercancel"]) addEventListener(t, up, true);
}

// ------------------------------------------------------------------ the computer's keys
const KNOB_KEYS = new Set(["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "PageUp", "PageDown", "Home", "End"]);
function keyboard() {
  // a piano from the lowest key (F): white keys on the home row, black above; sustain on Q, not Shift (Shift+Tab
  // and Shift+Enter would tap ENV, and two taps lock it)
  for (const c of Object.keys(KEYMAP)) delete KEYMAP[c];
  ["KeyA", "KeyW", "KeyS", "KeyE", "KeyD", "KeyR", "KeyF", "KeyG", "KeyY", "KeyH", "KeyU", "KeyJ", "KeyK", "KeyO", "KeyL", "KeyP",
   "Semicolon", "BracketLeft", "Quote"].forEach((c, i) => { KEYMAP[c] = { key: i }; });
  Object.assign(KEYMAP, { Space: { btn: BTN.PLAY }, Backspace: { btn: BTN.REC }, KeyQ: { btn: BTN.ENV },
                          Digit1: { btn: BTN.ARP }, Digit2: { btn: BTN.FX }, Escape: { btn: BTN.HOME },
                          KeyZ: { btn: BTN["OCT-"] }, KeyX: { btn: BTN["OCT+"] } });
  // only while the FM-1 has focus (single letters never get in the way of the rest of the page or a screen reader).
  // A control reached with Tab keeps Space and Enter for itself (it presses that control); one clicked with the
  // mouse or a finger does not, so Space stays PLAY after playing the panel with the mouse
  let clicked = null;
  controls.addEventListener("pointerdown", (e) => { clicked = e.target.closest(".hit"); }, true);
  addEventListener("keydown", (e) => { if (e.key === "Tab") clicked = null; }, true);
  controls.addEventListener("keydown", (e) => {
    if (!node || !shortcuts.checked || e.metaKey || e.ctrlKey || e.altKey) return;
    const hit = e.target.closest(".hit");
    if (hit && hit.classList.contains("knob") && KNOB_KEYS.has(e.key)) return;   // a knob turns with its keys
    if (hit && hit !== clicked && (e.key === " " || e.key === "Enter")) return;   // reached with Tab: presses it
    if (e.code === "ArrowUp" || e.code === "ArrowDown") {
      turn(knobs.SELECT, e.code === "ArrowUp" ? 1 : -1);
      e.preventDefault(); e.stopPropagation();
      return;
    }
    const m = KEYMAP[e.code];
    if (!m) return;
    e.preventDefault(); e.stopPropagation();
    if (!e.repeat) holdDown("k" + e.code, m.key !== undefined ? "key" : "btn", m.key !== undefined ? m.key : m.btn);
  }, true);
  // on the keys, Left and Right (Home, End) move to the next key
  keys.forEach((k, i) => k.hit.addEventListener("keydown", (e) => {
    const to = { ArrowLeft: i - 1, ArrowRight: i + 1, Home: 0, End: keys.length - 1 }[e.key];
    if (to === undefined || e.metaKey || e.ctrlKey || e.altKey) return;
    e.preventDefault(); e.stopPropagation();
    keys[Math.max(0, Math.min(keys.length - 1, to))].hit.focus();
  }));
}

// ------------------------------------------------------------------ names and tips
// what each control does, on hover (the lists below the FM-1 say the same)
const TIP = { FX: "Next knob bank (A to D); hold: set their controller numbers", SEL: "Keys page (scale)", ENV: "Hold: sustain; tap twice: lock it",
  LFO: "Hold: mod wheel", EDIT: "Sound page", GLO: "Setup page (tempo, clock)", HOME: "Play page", SAVE: "Save now", ARP: "Arp page",
  SEQ: "Sequencer page", PLAY: "Start / stop", REC: "All notes off", "OCT-": "Keys an octave down", "OCT+": "Keys an octave up",
  SELECT: "The page's pages: banks on Play, tracks on the Sequencer", ALGORITHM: "MIDI channel", PRESETS: "Program change (the sound follows)",
  MASTER: "Volume, or a controller (Sound page)",
  KNOB1: "The page's first value", KNOB2: "The page's second value", KNOB3: "The page's third value", KNOB4: "The page's fourth value" };
const NOTE = ["C", "C sharp", "D", "D sharp", "E", "F", "F sharp", "G", "G sharp", "A", "A sharp", "B"];
function names() {
  for (const [n, b] of Object.entries(buttons)) if (TIP[n]) { b.hit.title = TIP[n]; b.hit.setAttribute("aria-description", TIP[n]); }
  for (const [n, k] of Object.entries(knobs)) {
    const how = k.hit.getAttribute("aria-description");
    k.hit.title = TIP[n];
    k.hit.setAttribute("aria-description", n === "MASTER" ? TIP[n] : `${TIP[n]}. ${how}`);
  }
  // the keys by their notes, as they start (F3 to G5; OCT and the scales change what they play)
  keys.forEach((k, i) => {
    const m = 53 + i, n = k.name.match(/^(White|Black) key (\d+)/);
    k.name = `${NOTE[m % 12]} ${Math.floor(m / 12) - 1} key`;
    k.light = undefined;                 // (the page names it again, with its light, on the next frame)
    k.hit.setAttribute("aria-label", k.name);
    k.hit.setAttribute("aria-description", `${n[1]} key ${n[2]}${n[1] === "Black" ? ", a chord with a scale" : ""}. ` +
      "Space or Enter plays it, for as long as you hold it, at the keyboard velocity. Left and Right arrows: the next key.");
  });
}

// MASTER three-quarters up: sound at once
function masterStart() {
  master = 3072;
  const k = knobs.MASTER, pct = Math.round(master * 100 / 4096);
  k.angle = -135 + 270 * master / 4096;
  k.ind.setAttribute("transform", `rotate(${k.angle} ${k.x} ${k.y})`);
  k.hit.setAttribute("aria-valuenow", pct);
  k.hit.setAttribute("aria-valuetext", `${pct} percent`);
}

export function attach() {
  senseKeys();
  keyboard();
  names();
  masterStart();
}
