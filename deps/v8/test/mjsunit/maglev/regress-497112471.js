// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev-untagged-phis --allow-natives-syntax

// Creating an object with a AnyTagged field.
let o = { x : 42 };
o.x = "abc";

let h0 = %AllocateHeapNumberWithValue(42);
let h1 = %AllocateHeapNumberWithValue(27);

function foo(c) {
  let phi1 = c ? h0 : h1;
  phi1 + 2;
  let phi2 = c ? "abc" : phi1;
  o.x = phi2;
}

%PrepareFunctionForOptimization(foo);
foo(true);

%OptimizeMaglevOnNextCall(foo);
foo(true);

foo(false);
