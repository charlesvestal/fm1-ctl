#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""Make the CTL-1 site for GitHub Pages (https://charlesvestal.github.io/fm1-ctl/):

  index.html                     what CTL-1 is, and the ways in: try, install, manual, download, source
  img/                           screenshots (docs/img)
  emu/                           CTL-1 in the browser (app/build/emu, from ./build.sh --web), with
                                 fm1-reader's words.js, tts.js and eSpeak NG for its screen reader
  words/                         the screen reader's words files (app/web/words, from app/words.tsv)
  install/index.html             the web installer (web/ctl_installer.html, with fm1-base's fm1pkg.js and
                                 fm1ota.js and the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/        the manual (fm1-base's tools/manual_html.py, from docs/MANUAL.md)
  firmware/ctl-VERSION.fwsc      the package the installer writes; also the download

  tools/make_pages.py app/build/ctl-0.9.fwsc 0.9 OUT_DIR

The package must be a release build (./build.sh --release X.Y): its identity, FM-1_6XXYYZZ, is what the
installer checks the download against and what the FM-1 reports after the install."""
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
BASE = SRC / "base"                                       # fm1-base (git submodule)
sys.path.insert(0, str(BASE / "tools"))
from make_site import product_of, strip_module  # noqa: E402  (the package format)

REPO = "https://github.com/charlesvestal/fm1-ctl"

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CTL-1 · a MIDI controller for the FM-1</title>
<meta name="description" content="CTL-1 turns the M-VAVE FM-1 into a MIDI controller, with notes in any scale and its chords, four banks of knobs, an arpeggiator, a Euclidean drum sequencer and General MIDI sounds of its own.">
<meta name="theme-color" content="#fbf8f1">
<style>
/* CTL-1's paper and ink, as its screen: warm white paper, ink-blue type, vermilion for what is played,
   mustard for controllers */
:root { --paper: #fbf8f1; --sheet: #efe9dc; --rule: #ddd4c2; --ink: #1e2b5a; --muted: #5d6585;
        --hot: #e5482e; --hot-t: #f8d3c9; --cool: #d9a21b; --cool-t: #f5e4b8; color-scheme: light;
        --hot-ink: #c23a22; }   /* the vermilion behind or as text: 4.5:1 */
body { background: var(--paper); color: var(--ink); margin: 0; padding-inline: 16px; padding-block: 48px 64px;
       font: 400 17px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 42rem; margin: 0 auto; }
h1 { font-size: 3.2rem; font-weight: 700; line-height: 1; letter-spacing: .02em; margin: 0 0 .3rem; }
.sub { font-weight: 700; font-size: 1rem; margin: 0 0 1.2rem; padding-bottom: .8rem; border-bottom: 3px solid var(--hot); }
h2 { font-size: 1.15rem; font-weight: 700; margin: 2.4rem 0 .6rem; padding-top: .6rem; border-top: 2px solid var(--ink); }
.lede { font-size: 1.15rem; margin: 0 0 1.6rem; max-width: 36rem; }
.shots { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin: 0 0 1.8rem; }
.shots img { width: 100%; height: auto; image-rendering: pixelated; border-radius: 4px; box-shadow: 0 0 0 1px var(--rule); }
.ways { display: grid; gap: 10px; margin: 0 0 1rem; }
.ways a { display: block; text-decoration: none; color: var(--ink); background: var(--sheet); border-radius: 8px; padding: 12px 16px; }
.ways a:first-child { background: var(--hot-ink); color: #fff; }
.ways a:first-child span { color: #fff; }
.ways a:hover, .ways a:focus-visible { background: var(--cool-t); color: var(--ink); }
:focus-visible { outline: 3px solid var(--ink); outline-offset: 2px; }
.ways a:first-child:hover span, .ways a:first-child:focus-visible span { color: var(--muted); }
.ways strong { font-weight: 700; font-size: 1.15rem; display: block; }
.ways span { color: var(--muted); font-size: .95rem; }
.ways em.v { font-style: normal; font-weight: 400; }
ul.feat { padding-left: 1.2rem; margin: 0 0 1rem; }
ul.feat li { margin: 0 0 .4rem; }
ul.feat li::marker { color: var(--hot); }
p.small { color: var(--muted); font-size: .9rem; }
a { color: var(--ink); text-decoration-color: var(--hot); text-decoration-thickness: 2px; text-underline-offset: 3px; }
code { font-size: .9em; }
</style>
</head>
<body>
<main>
<h1>CTL-1</h1>
<p class="sub">A MIDI controller for the M-VAVE FM-1</p>
<p class="lede">CTL-1 turns your FM-1's keys, knobs and buttons into a MIDI controller. Play notes in any scale, with
its chords under the black keys. Send controllers from four banks of knobs, and run an arpeggiator and a Euclidean drum
sequencer, over USB and Bluetooth. It has General MIDI sounds of its own, so it plays with nothing connected.</p>
<div class="shots">
  <img src="img/screen-play.png" width="240" height="240" alt="CTL-1's Play page: Note E4 100, a chord lit on the keys; bank A: CC 74 at 52, CC 71 at 70, CC 1 at 0, CC 7 at 100">
  <img src="img/screen-keys.png" width="240" height="240" alt="CTL-1's Keys page: D Dorian, notes D E F G A B C; white keys the scale, black keys its chords">
  <img src="img/screen-seq.png" width="240" height="240" alt="CTL-1's Sequencer page, playing at 120: rows of steps for Kick, Snare, Hat and Clap; track 2: 16 steps, 2 hits, rotate 4">
</div>
<nav class="ways" aria-label="Get CTL-1">
  <a href="emu/"><strong>Try it in the browser</strong><span>Play it right here, with sound, using a mouse, touch or the keyboard. It runs the same code as the FM-1, and in Chrome or Edge it can play your MIDI ports.</span></a>
  <a href="install/"><strong>Install <em class="v">v__VERSION__</em></strong><span>Connect your FM-1 by USB and install from Chrome or Edge. There's nothing to install on your computer.</span></a>
  <a href="manual/"><strong>Manual</strong><span>Get started in a few minutes, then look up everything else.</span></a>
  <a href="firmware/__PKG__"><strong>Download <em class="v">__PKG__</em></strong><span>For the command-line installer: <code>python3 base/tools/fm1_install.py __PKG__</code></span></a>
  <a href="__REPO__"><strong>Source</strong><span>On GitHub under MPL-2.0. Built on fm1-base and felucca-bsp, which grew out of Felucca by Leo Kuroshita (Hügelton Instruments).</span></a>
</nav>

<h2>What it does</h2>
<ul class="feat">
  <li>Play the 27 <b>keys</b> across ±3 octaves. Pick a scale (nine of them, on any root), and the white keys play its notes while the black keys play its chords. Hold ENV for sustain, or tap it twice to lock it, and hold LFO for the mod wheel. The keys read velocity and aftertouch by themselves, with no change to the hardware.</li>
  <li>Turn the <b>knobs</b> to send four controllers in each of four banks, 16 in all, each with its own number. MASTER can send one too.</li>
  <li>Hold a chord for the <b>arpeggiator</b>. It plays up, down, up-down, at random or as played, from 1/4 to 1/32, over up to three octaves, with latch.</li>
  <li>Build beats on the <b>sequencer</b>: four drum tracks of Euclidean rhythms on their own channel, which you can edit on the keys.</li>
  <li>Run everything on its own <b>clock</b> and send it as MIDI clock with start and stop, or follow yours.</li>
  <li>Play the 128 General MIDI <b>sounds</b> and the drum kit (Sonivox EAS) through the FM-1's speaker and headphones. Turn PRESETS to pick one.</li>
</ul>
<p>MIDI goes out and comes in over USB and Bluetooth, and MIDI from the TRS jack is passed on.</p>
<p class="small">To go back to the official firmware, use "Back to the stock firmware" in the installer, or
M-VAVE's own updater, M-UPGRADE, from <a href="https://www.m-vave.com/download">m-vave.com/download</a>.
M-VAVE and FM-1 are trademarks of their owners. CTL-1 is not affiliated with M-VAVE.</p>
</main>
</body>
</html>
"""


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_6\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (./build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = pkg.name
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "firmware"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    # the installer
    page = (SRC / "web" / "ctl_installer.html").read_text(encoding="utf-8")
    lib = strip_module((BASE / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((BASE / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"ctl_installer.html must contain {mark} once")
    (out / "install" / "index.html").write_text(page.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    # the manual (a complete page; its images beside it)
    subprocess.run([sys.executable, str(BASE / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html"), "--name", "CTL-1", "--repo", REPO,
                    "--tagline", "A MIDI controller for the FM-1"], check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the landing page and its screenshots
    shutil.copytree(SRC / "docs" / "img", out / "img")
    (out / "index.html").write_text(LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name)
                                    .replace("__REPO__", REPO), encoding="utf-8")
    emu = SRC / "app" / "build" / "emu"
    if not (emu / "app.wasm").exists():
        raise SystemExit("no app/build/emu/app.wasm: run ./build.sh --web (needs Emscripten)")
    if not (emu / "reader" / "words.js").exists() or not (emu / "reader" / "espeak" / "espeak.wasm").exists():
        raise SystemExit("no app/build/emu/reader/: the emulator's screen reader comes from fm1-reader "
                         "(./build.sh --web copies it from an fm1-reader checkout beside this one, or set FM1_READER)")
    shutil.copytree(emu, out / "emu")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "app" / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, install/ ({product}), manual/, emu/, words/, firmware/{name} ({len(raw)} B)")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
