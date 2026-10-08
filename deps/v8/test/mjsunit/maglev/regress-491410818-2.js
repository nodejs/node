// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --no-maglev-loop-peeling

// HeapObject field.
let o = { x : "abc" };
o.x = [];

const large_smi = 0x3ffffffa; // Close to kSmiMaxValue.

function foo(n) {
  let phi1 = large_smi;
  for (let i = 0; i < n; i++) {
    phi1++; // Smi-feedback, will allow {phi1} to be untagged to Int32.
  }
  phi1 | 0; // Will insert a CheckedSmiUntag. If SetUseRequiresSmi hasn't been
            // called on {phi1}, then this CheckedSmiUntag can be elided after
            // untagging(wrong).
  let phi2 = n ? phi1 : 42;
  o.x = phi2; // Should call SetUseRequiresSmi on {phi2} and recursively on
              // {phi1} as well.
}

%PrepareFunctionForOptimization(foo);
foo(3);
assertEquals(0x3ffffffa+3, o.x);

%OptimizeMaglevOnNextCall(foo);
foo(4);
assertEquals(0x3ffffffa+4, o.x);

// Making {phi1} overflow the Smi range (still within Int32 range though).
foo(10);
assertEquals(0x3ffffffa+10, o.x);
