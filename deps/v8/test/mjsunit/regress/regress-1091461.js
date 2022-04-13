// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, b) {
  let x = a + b;
}
function test() {
  try {
    foo(1n, 1n);
  } catch (e) {}
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
