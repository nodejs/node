// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x) {
  let o = { x : x };
  if (x == 42) {
    o.x = 17;
  } else {
    o.x = 17;
  }
  // When merging here, there is no need to insert a Phi, since both
  // predecessors have the same value.
  %AssertEscapeAnalysisElided(o);
  return o.x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(17, foo(42));
assertEquals(17, foo(41));

%OptimizeFunctionOnNextCall(foo);
assertEquals(17, foo(42));
assertEquals(17, foo(41));
