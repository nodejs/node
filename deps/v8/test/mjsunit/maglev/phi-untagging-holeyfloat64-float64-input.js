// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

let hf64_arr = [1.1, /*hole*/ , 3.3, /*hole*/, /*hole*/];

function foo(n) {
  let ret = 17.25; // float64 input
  let acc = 3.333; // float64 input
  for (let i = 0; i < n; i++) {
    acc += ret; // float64 use for {ret} and {acc} inside the loop
    let hf64_phi = hf64_arr[i]; // holeyfloat64 load
    if (i == 3) {
      hf64_phi = -Math.abs(hf64_phi); // Will produce a hole if input is a hole
    }
    ret = hf64_phi; // will be a holeyfloat64 phi
  }
  return ret;
}

%PrepareFunctionForOptimization(foo);
assertEquals(3.3, foo(3));
assertEquals(NaN, foo(4));

%OptimizeMaglevOnNextCall(foo);
assertEquals(3.3, foo(3));
assertEquals(NaN, foo(4));
