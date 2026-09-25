// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let hf64_arr = [ 1.5, /*hole*/, 3.5 ];
hf64_arr[0] = 3.2; // making sure {hf64_arr} isn't const.

function foo(c, o, idx) {
  let phi = c ? o.x : hf64_arr[idx];
  return phi ^ 4;
}

let o = { x : 42 };
o.x = 25;

%PrepareFunctionForOptimization(foo);
assertEquals(29, foo(true, o));
assertEquals(7, foo(false, o, 0));
assertEquals(4, foo(false, o, 1));

%OptimizeFunctionOnNextCall(foo);
assertEquals(29, foo(true, o));
assertEquals(7, foo(false, o, 0));
assertEquals(4, foo(false, o, 1));
