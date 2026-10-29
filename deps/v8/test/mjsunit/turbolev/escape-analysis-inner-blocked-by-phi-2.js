// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbofan

function foo(x, deopt) {
  let outer_obj = { x : x, y : x };
  %AssertEscapeAnalysisElided(outer_obj);

  for (let i = 0; i < 42; i++) {
    let inner_obj = { i : i };
    // {inner_obj} is stored in `outer_obj.x`, and given that {outer_obj}
    // doesn't escape, we'll create a new loop phi for its `x` field, which
    // means that {inner_obj} will flow into a loop phi, which is currently
    // considered an escaping use.
    outer_obj.x = inner_obj;
  }
  if (deopt) { return outer_obj.x; }

  return outer_obj.y;
}

%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(42, false));
assertEquals(42, foo(42, false));

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo(42, false));
assertOptimized(foo);

// Triggering deopt.
assertEquals({ i : 41 }, foo(42, true));
assertUnoptimized(foo);
