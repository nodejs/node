// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let arr = [0.5];
function foo(a, b) {
  a + 0.5;
  if (b) {
    arr[0] = a;
  }
}
%PrepareFunctionForOptimization(foo);
foo(2.3, true);
foo();
%OptimizeFunctionOnNextCall(foo);
foo(undefined, true);
assertEquals(undefined, arr[0]);
