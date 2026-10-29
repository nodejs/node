// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x) {
  return 1 / (x + x);
}

function foo(n) {
  var y = 2147483648;
  var x = bar(y)
  for (; n > 0; --n) {
    x = bar(y);
    y = -0;
  }
  return x;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(1);
%OptimizeFunctionOnNextCall(foo);
assertEquals(-Infinity, foo(2));
