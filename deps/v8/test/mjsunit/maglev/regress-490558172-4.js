// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --maglev
// Flags: --no-maglev-loop-peeling

const max_smi = 0x3fffffff;
const non_smi_int32 = max_smi + 1;
let cst = max_smi-1;

let o1 = { x : 17 };
o1.x = 25;

let o2 = { y : "abc" };
o2.y = [];

function foo(c, n) {
  let phi = 0x3ffffffc;

  for (let i = 0; i < n; i++) {
    phi++;
  }

  if (c) {
    cst = phi;
    o1.x = phi;
  } else {
    let p2 = c ? phi : non_smi_int32;
    o2.y = p2;
  }
}

%PrepareFunctionForOptimization(foo);
foo(true, 3);
foo(false, 3);

%OptimizeMaglevOnNextCall(foo);
foo(true, 3);
foo(false, 3);
