// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --maglev

const non_smi_int32 = 0x40000000;
let cst = 47;

let o1 = { x : 17 };
o1.x = 25;

let o2 = { y : "abc" };
o2.y = [];

function foo(c) {
  let phi = 42;
  for (let i = 0; i < 10; i++) {
    if (i == 5) {
      cst = phi;  // Will insert a CheckValueEqualsInt32
      // Requires Smi.
      o1.x = phi; // Will not insert a CheckSmi because CheckValueEqualsInt32
                  // registers an Smi-sized Int32 constant alternative.

    }
    phi++;
  }
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

assertEquals(47, cst);
assertEquals(47, o1.x);
assertEquals(non_smi_int32, o2.y);

foo(true);
assertEquals(47, cst);
assertEquals(47, o1.x);
assertEquals(52, o2.y);
