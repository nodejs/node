// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --sparkplug

// CallSite.getFunction() must not expose a JSFunction across security-token
// boundaries, regardless of which tier the frame is in.

const attackerRealm = Realm.create();

// Define the callback in the attacker realm so the captured stack is
// summarised against that realm's security token.
const attackerCallback = Realm.eval(attackerRealm, `
  Error.prepareStackTrace = (_e, frames) =>
      frames.map(f => f.getFunction());
  (function attackerCallback() {
    const err = {};
    Error.captureStackTrace(err);
    return err.stack;
  });
`);

// CallSite.getFunction() returns undefined for strict-mode frames, so the
// bridge must be sloppy to actually expose its function on the stack.
function trustedBridge(callback) {
  return callback();
}

%CompileBaseline(trustedBridge);
assertTrue(isBaseline(trustedBridge));

const frames = trustedBridge(attackerCallback);

for (const fn of frames) {
  assertNotEquals(trustedBridge, fn);
}
