// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis
// Flags:  --no-maglev-loop-peeling --verify-heap

const smi_max = 0x3fffffff;

let o = { x : 29 }
o.x = 17;

function foo(c, n) {
  let phi1 = smi_max - 3;
  for (let i = 0; i < n; i++) {
    phi1++;
  }
  phi1 | 0; // Inserting CheckedSmiUntag.

  let phi2 = c ? phi1 : n;
  o.x = phi2;
}

%PrepareFunctionForOptimization(foo);
foo(true, 2);
foo(true, 2);

%OptimizeMaglevOnNextCall(foo);
foo(true, 2);

// Making {phi2} be a HeapObject.
foo(true, 10);
