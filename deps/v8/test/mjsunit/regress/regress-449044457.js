// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arr) {
  let v = 4.65;
    for (let i = 0; i < 1; i++) {
      v = arr[i];
    }
  return [v];
}

let a = [/*hole*/, 1.1];

%PrepareFunctionForOptimization(foo);
 foo(a);
%OptimizeMaglevOnNextCall(foo);
let r = foo(a);

function removeHoles(a) {
  return a.filter(x => true);
}
assertEquals(1, removeHoles(r).length);
