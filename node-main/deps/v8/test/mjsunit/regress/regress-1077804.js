// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  return bar();
}

function bar(a, b) {
  return a + b;
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
%PrepareFunctionForOptimization(bar);
%OptimizeFunctionOnNextCall(bar);
bar(2n, 2n);
assertTrue(Number.isNaN(foo()));
