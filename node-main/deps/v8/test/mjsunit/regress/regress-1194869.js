// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --interrupt-budget=100

function f() {
  return "".indexOf("", 2);
}

%PrepareFunctionForOptimization(f)
assertEquals(f(), 0);
assertEquals(f(), 0);
%OptimizeFunctionOnNextCall(f)
assertEquals(f(), 0);
assertEquals(f(), 0);

function g() {
  return "".indexOf("", 2);
}
for (let i = 0; i < 191; i++) {
  // Expect a natural optimization here due to low interrupt budget.
  assertEquals(g(), 0);
}
