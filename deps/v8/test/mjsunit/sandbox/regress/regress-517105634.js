// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --sandbox-testing --memory-corruption-via-watchpoints
// Flags: --allow-natives-syntax

function trigger(obj) {
  return obj.a;
}
const obj = {a: 1.1};
%PrepareFunctionForOptimization(trigger);
trigger(obj);
%OptimizeFunctionOnNextCall(trigger);
trigger(obj);

// Target the double field access.
Sandbox.markForCorruptionOnAccess(obj, 12);

for (let i = 0; i < 100; ++i) {
  trigger(obj);
}
