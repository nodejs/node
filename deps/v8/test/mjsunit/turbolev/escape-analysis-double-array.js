// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x, deopt) {
  let arr = [1.5, x, 3.5];
  %AssertEscapeAnalysisElided(arr);

  if (deopt) {
    x+3; // no feedback ==> unconditional deopt.
    return arr;
  }

  return arr[2];
}

%PrepareFunctionForOptimization(foo);
assertEquals(3.5, foo(2.5, false));
assertEquals(3.5, foo(2.5, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(3.5, foo(2.5, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals([1.5, 2.5, 3.5], foo(2.5, true));
assertUnoptimized(foo);
