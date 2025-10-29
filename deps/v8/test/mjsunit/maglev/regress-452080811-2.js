// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let arr = [1.1, , 3.3, 4.4];
arr[0] = 1.15; // Making sure {arr} isn't const.

function foo(i) {
  let v = arr[i];
  return v + -0.0;
}

%PrepareFunctionForOptimization(foo);
foo(0); // 1.15
assertEquals(NaN, foo(1)); // NaN

%OptimizeFunctionOnNextCall(foo);
foo(0); // 1.15
assertEquals(NaN, foo(1)); // NaN
