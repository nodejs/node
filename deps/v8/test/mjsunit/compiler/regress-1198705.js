// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(x) {
  x = (x|0) * 2**52;
  x = Math.min(Math.max(x, 0), 2**52);
  return (x + x)|0;
}

%PrepareFunctionForOptimization(bar);
assertEquals(0, bar(0));
%OptimizeFunctionOnNextCall(bar);

function foo() {
  return bar(1);
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
