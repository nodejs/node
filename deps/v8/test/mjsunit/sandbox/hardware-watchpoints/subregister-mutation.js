// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --memory-corruption-via-watchpoints --sandbox-testing
// Flags: --allow-natives-syntax

function trigger(s) {
  return s.charCodeAt(0);
}

let s = 'AAAAA';
%PrepareFunctionForOptimization(trigger);
trigger(s);
%OptimizeFunctionOnNextCall(trigger);
trigger(s);

// Mark string object for corruption on access.
Sandbox.markForCorruptionOnAccess(s, 12);

let seen = new Set();
for (let i = 0; i < 100; i++) {
  let val = trigger(s);
  seen.add(val);
  // Verify 8-bit zero-extended bounds.
  assertTrue(val >= 0 && val <= 255, `Out of bounds byte load: ${val}`);
}
