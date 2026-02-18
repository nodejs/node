// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(b) {
  let phi = 0;
  var x = 0x4e000000;
  for (let i = 0; i < 200; ++i) {
    if (i == 100 && b) {
      phi = x;
    }
    phi + 2;
    if (i == 10 && b) {
      %OptimizeOsr();
    }
  }
  return phi;
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(false));

// Triggering OSR and non-Smi value for the phi.
assertEquals(0x4e000000, foo(true));
