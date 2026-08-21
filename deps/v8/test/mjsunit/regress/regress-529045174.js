// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle --harmony-shadow-realm --no-fail

// JS_BUNDLE_MODULE:mod.mjs
let outerFunc;
export function register(fn) { outerFunc = fn; }
export function callOuter() { outerFunc(); }

// JS_BUNDLE_SCRIPT
let r = new ShadowRealm();
(async () => {
  try {
    let register = await r.importValue("mod.mjs", "register");
    register(() => {
      d8.terminateNow();
    });

    r.evaluate(`
      import("mod.mjs").then(m => m.callOuter());
    `);
  } catch (e) {
  }
})();
