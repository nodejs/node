// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo() {
  const arr = new Array(3);
  for (let i = 0; i < 1; i++) {
    arr[i] = i;
  }
  return arr;
}

%PrepareFunctionForOptimization(foo);
assertEquals([0,,,], foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals([0,,,], foo());
