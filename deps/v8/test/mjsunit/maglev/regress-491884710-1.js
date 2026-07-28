// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis --verify-heap

const smi_max = 0x3fffffff;

let o = { x : 29 }
o.x = 17;

function foo(c, n) {
  let phi1 = c ? n + 1 : 42;
  phi1 | 0; // Inserting CheckedSmiUntag.

  let phi2 = c ? phi1 : 42;
  o.x = phi2;
}

%PrepareFunctionForOptimization(foo);
foo(true, 19);
foo(true, 19);

%OptimizeMaglevOnNextCall(foo);
foo(true, 19);
foo(true, smi_max);
