// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function foo(n) {
  // Note that the following StaticAssert relies on the fact that Turbolev will
  // optimize n+0 to n, leading to the comparison being `n == n`, which will
  // then be optimized to `true`.
  %TurbofanStaticAssert(n+0 == n);
}

%PrepareFunctionForOptimization(foo);
foo(42);

%OptimizeFunctionOnNextCall(foo);
foo(42);
assertOptimized(foo);
