// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis


let o = { x : "abc" };
o.x = []; // Making non-string, non-const, but still HeapObject

let string_wrapper = new String("abc");

function foo(c) {
  let phi1 = c ? 42 : 0x40000000;
  phi1 + 1; // inserting smi check

  let phi2 = c ? string_wrapper : phi1;
  o.x = phi2;

  return "" + phi2;
}

%PrepareFunctionForOptimization(foo);
assertEquals("abc", foo(true));

%OptimizeMaglevOnNextCall(foo);
assertEquals("abc", foo(true));

assertEquals(""+0x40000000, foo(false));
