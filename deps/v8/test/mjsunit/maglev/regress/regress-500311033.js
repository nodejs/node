// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-inlining

// Throwable nodes inside an eagerly-inlined callee whose nearest enclosing try
// is in the caller must keep the catch handler's known node aspects in sync
// with side effects performed inside the callee.

(function ThrowFromInlinedCallee() {
  let target = null;
  const thrower = {
    get y() {
      if (target !== null) target.a = {};
      throw 0;
    },
  };

  function inner() { thrower.y; }
  function outer(o) {
    o.a;
    try {
      inner();
    } catch (e) {
      return o.a + 1;
    }
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(outer);
  assertEquals(2, outer({a: 1}));
  assertEquals(2, outer({a: 1}));
  %OptimizeMaglevOnNextCall(outer);
  assertEquals(2, outer({a: 1}));
  const o = {a: 1};
  target = o;
  assertEquals('[object Object]1', outer(o));
})();

(function ThrowFromNestedInlinedCallee() {
  let target = null;
  const thrower = {
    get y() {
      if (target !== null) target.a = {};
      throw 0;
    },
  };

  function inner() { thrower.y; }
  function middle() { inner(); }
  function outer(o) {
    o.a;
    try {
      middle();
    } catch (e) {
      return o.a + 1;
    }
  }

  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(middle);
  %PrepareFunctionForOptimization(outer);
  assertEquals(2, outer({a: 1}));
  assertEquals(2, outer({a: 1}));
  %OptimizeMaglevOnNextCall(outer);
  assertEquals(2, outer({a: 1}));
  const o = {a: 1};
  target = o;
  assertEquals('[object Object]1', outer(o));
})();
