// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

let cst = 11;

let o = { x : 17 };
o.x = 25;

function foo(c) {
  for (let i = 0; i < 42; i++) {
    if (i == 11) {
      cst = i; // Will insert a CheckValueEqualsInt32
      o.x = i; // Will not insert a CheckSmi because CheckValueEqualsInt32
               // registers an Smi-sized Int32 constant alternative.
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo(true);
foo(true);

o.x = 42;

%OptimizeMaglevOnNextCall(foo);
foo(true);

assertEquals(11, cst);
assertEquals(11, o.x);
