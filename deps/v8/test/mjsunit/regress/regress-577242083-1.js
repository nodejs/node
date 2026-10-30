// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --maglev-non-eager-inlining --allow-natives-syntax

let ta = new Int32Array();
function return_zero() {
  return 0;
}
%PrepareFunctionForOptimization(return_zero);
%DisableEagerInlining(return_zero);

function foo() {
  let phi = 0;
  for (let i = 0; i < 2; i++) {
    ta[phi];
    let zero = return_zero();
    let y = phi | zero;
    "abc".startsWith("a", y);
    let z = y + 0.5;
    phi = -2147483649;
  }
}
%PrepareFunctionForOptimization(foo);

foo();

%OptimizeMaglevOnNextCall(foo);
foo();
