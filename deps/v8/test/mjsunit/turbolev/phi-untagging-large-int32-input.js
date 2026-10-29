// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let o = { x : "abc" };
o.x = []; // Making non-string, non-const, but still HeapObject

function foo(c1, c2) {
  let phi1 = c1 ? 42 : 0x40000000;
  phi1 + 1; // inserting smi check

  let phi2 = c2 ? true : phi1;
  o.x = phi2;
}

%PrepareFunctionForOptimization(foo);
foo(true, true);
foo(true, true);

%OptimizeMaglevOnNextCall(foo);
foo(true, true);
foo(true, true);

foo(false, false);
