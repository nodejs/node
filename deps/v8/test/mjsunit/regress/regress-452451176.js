// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(i, stop_target) {
  if (i == stop_target) { throw "blah" }
}
%NeverOptimizeFunction(bar);

function foo(stop_target) {
  let t = 0;
  try {
    for (let i = 1; i != -2147483648; i = (i + -1) & 0xffffffff) {
      t ^= i;
      bar(i, stop_target);
    }
  } catch(e) {  }
  return t;
}

%PrepareFunctionForOptimization(foo);
assertEquals(-6, foo(-5));
assertEquals(-6, foo(-5));

%OptimizeFunctionOnNextCall(foo);
assertEquals(-6, foo(-5));
