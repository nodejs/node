// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling

// This test should crash when run with Turbolev: the %AssertPeeled() marker sits
// in the OUTER loop of a nested loop, and the peeler only peels innermost loops,
// so the marker is never stripped and lowers to a failing StaticAssert. The
// reason this test exists is to notice if %AssertPeeled starts being silently
// ignored (e.g. the peeler strips markers it shouldn't, or the Turbolev lowering
// is dropped): if that happens this test will unexpectedly pass.

function foo(n) {
  let s = 0;
  for (let i = 0; i < n; i++) {
    %AssertPeeled();
    for (let j = 0; j < n; j++) {
      s += j;
    }
  }
  return s;
}

%PrepareFunctionForOptimization(foo);
foo(10);

%OptimizeFunctionOnNextCall(foo);
foo(10);
assertOptimized(foo);
