// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// Regression test: storing undefined (or any oddball) into a Uint8ClampedArray
// used to eager-deopt the Maglev code with reason "not a Number", causing a
// deopt loop on every re-optimization.

var ta = new Uint8ClampedArray(10);
function foo(ta, i, v) {
  ta[i] = v;
}

%PrepareFunctionForOptimization(foo);
foo(ta, 0, 1);
%OptimizeMaglevOnNextCall(foo);
foo(ta, 0, undefined);
foo(ta, 0, undefined);
foo(ta, 0, undefined);
foo(ta, 0, undefined);

// ToNumber(undefined) is NaN, which clamps to 0.
assertEquals(0, ta[0]);
// Without the fix this would deopt on every call, so the function would no
// longer be Maglev-optimized here.
assertTrue(isMaglevved(foo));
