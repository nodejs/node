// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let caught;

async function f() {
  let x = undefined;
  try {
    for (let i = 0; i < 5; i++) {
      for await (const v of 0) {
      }
    }
  } catch (e) {
    caught = x;
  }
  [x];
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeMaglevOnNextCall(f);
caught = 42;
f();
assertEquals(undefined, caught);
