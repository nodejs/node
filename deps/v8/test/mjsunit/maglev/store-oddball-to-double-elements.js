// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

let o = [0.5]

function foo(x, store) {
  x + 0.5; // Give x a float64_alternative
  if (store) {
    o[0] = x;
  }
}

%PrepareFunctionForOptimization(foo);
// Warm up the add with NumberOrOddball feedback, but keep the store as
// a double store.
foo(2.3, true);
foo(undefined, false);

%OptimizeMaglevOnNextCall(foo);
// Storing a number should work.
foo(2.3, true);
assertEquals(2.3, o[0]);
// Storing an oddball should work and not store the ToNumber of that oddball.
foo(undefined, true);
assertEquals(undefined, o[0]);
