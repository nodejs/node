// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-always-turbofan

function foo(x) {
  return BigInt(x);
}

function bar() {
  for (let i = 0; i < 1; ++i) {
    // The empty closure weakens the range of {i} to infinity in several
    // iterations.
    function t() { }
    foo(i);
  }
}

%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
