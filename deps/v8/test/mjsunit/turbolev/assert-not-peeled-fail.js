// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// This test should crash when run with Turbolev: the %AssertNotPeeled() marker
// sits in a plain innermost loop, which the peeler does peel, so reaching it
// during peeling fails the compile. The reason this test exists is to notice if
// %AssertNotPeeled starts being silently ignored (e.g. the peeler stops
// checking the marker): if that happens this test will unexpectedly pass.

function foo(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertNotPeeled();
    s += i;
  }
  return s;
}

%PrepareFunctionForOptimization(foo);
foo(10);

%OptimizeFunctionOnNextCall(foo);
foo(10);
assertOptimized(foo);
