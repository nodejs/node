// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x, b, deopt) {
  let outer_obj = { x : x, y : x };
  %AssertEscapeAnalysisElided(outer_obj);

  if (b) {
    // {inner_obj} is stored in `outer_obj.x`, and given that {outer_obj}
    // doesn't escape, we'll create a phi for its `x` field after this if (since
    // when the if isn't taken it will keep its initial value), which means that
    // {inner_obj} will flow into a phi, which is currently considered an
    // escaping use.
    let inner_obj = { i : x };
    outer_obj.x = inner_obj;
  }

  if (deopt) { return outer_obj.x; }

  return outer_obj.y;
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(42, true, false));
assertEquals(42, foo(42, true, false));
assertEquals(42, foo(42, false, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(42, true, false));
assertEquals(42, foo(42, false, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals({ i : 42 }, foo(42, true, true));
assertUnoptimized(foo);
