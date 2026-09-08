// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --maglev

const non_smi_int32 = 0x40000000;
let cst = 52;

let o1 = { x : 17 };
o1.x = 25;

let o2 = { y : "abc" };
o2.y = [];

function foo(c) {
  let phi = 42;
  for (let i = 0; i < 10; i++) {
    phi++;
  }
  cst = phi;
  o1.x = phi;
  let phi2 = c ? phi : non_smi_int32;
  // Requires HeapNumber
  o2.y = phi2;
}

%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);

%OptimizeMaglevOnNextCall(foo);
foo(false);
assertOptimized(foo);

assertEquals(52, cst);
assertEquals(52, o1.x);
assertEquals(non_smi_int32, o2.y);

foo(true);
assertEquals(52, cst);
assertEquals(52, o1.x);
assertEquals(52, o2.y);
