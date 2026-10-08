// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan
// Flags: --no-optimize-maglev-optimizes-to-turbofan

function f(a, c, b) {
  let bitwise = a | c;
  let phi = b ? bitwise : 42;

  // Smi feedback for ToNumber makes phi untagging insert CheckInt32IsSmi for
  // the Int32 phi. The check must ignore the high 32 bits and only validate the
  // Int32 payload.
  let number = +phi;
  if (phi < 100) return number;
  return 0;
}

%PrepareFunctionForOptimization(f);
assertEquals(42, f(0, -1, false));
assertEquals(-1, f(0, -1, true));
assertEquals(42, f(0, -1, false));
assertEquals(-1, f(0, -1, true));

%OptimizeMaglevOnNextCall(f);
assertEquals(42, f(0, -1, false));
assertTrue(isMaglevved(f));

assertEquals(-1, f(0, -1, true));
assertTrue(isMaglevved(f));
