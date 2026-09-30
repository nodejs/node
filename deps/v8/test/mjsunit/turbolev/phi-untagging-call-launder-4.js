// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

function make_phi(c, vtrue, vfalse) {
  return c ? vtrue : vfalse;
}

let o = { x : "abc"};
let H0 = %AllocateHeapNumberWithValue(42);
let H1 = %AllocateHeapNumberWithValue(27);

function foo(c) {
  let phi1 = make_phi(c, H0, H1);
  phi1 + 2;
  let phi2 = make_phi(c, "abc", phi1);
  o.x = phi2;
}

%PrepareFunctionForOptimization(make_phi);
%PrepareFunctionForOptimization(foo);
foo(true);

%OptimizeFunctionOnNextCall(foo);
foo(false);
