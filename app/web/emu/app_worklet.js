// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2026 Charles Vestal
// CTL-1's own part of fm1-base's emulator worklet (base/web/emu/worklet.js): a key with the velocity it lands with
// and how hard it is pressed (app.js: { type: "app", key, vel 1..127 or 0 up, press 0..1000 }) to the firmware's
// key sensing (web_key: velocity and aftertouch with Touch on).
globalThis.fm1App = {
  message(fm1, m) {
    if (m.key !== undefined) fm1.ex.web_key(m.key >>> 0, m.vel >>> 0, m.press >>> 0);
  },
};
