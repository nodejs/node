// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

// The inlined Array.prototype.sort deopt continuations restart the sort from
// the elements snapshot.  When the deopt happens after comparefn returned but
// before its result was converted, the continuation must still apply
// ToNumber to that result instead of dropping it: a stateful comparefn that
// returns a BigInt only once otherwise has its TypeError swallowed, and
// valueOf side effects go missing.

// State lives in an array so that changing it does not invalidate the
// optimized code through script context const tracking.
const state = [0, 0];  // [calls, valueOf calls]

function opt(f) {
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 3; i++) { state[0] = 0; f(); }
  %OptimizeFunctionOnNextCall(f);
  state[0] = 0; f();
  assertOptimized(f);
}
%NeverOptimizeFunction(opt);

// Lazy deopt at the comparefn call boundary with a pending BigInt result.
(function TestLazyDeoptPendingBigInt() {
  let armed = false;
  function cmp(a, b) {
    state[0]++;
    if (armed && state[0] === 1) {
      %DeoptimizeFunction(f);
      return 1n;
    }
    return a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);

  armed = true;
  state[0] = 0;
  assertThrows(f, TypeError);
  assertEquals(1, state[0]);
})();

// Eager deopt from the Number speculation on the comparefn result.  The
// result is loaded from a PACKED_ELEMENTS array so that it is an untyped
// tagged value with a non-BigInt warmup history.
(function TestEagerDeoptPendingBigInt() {
  const results = [0, {}];
  function cmp(a, b) {
    state[0]++;
    return state[0] === 1 ? results[0] : a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);

  results[0] = 1n;
  state[0] = 0;
  assertThrows(f, TypeError);
  assertEquals(1, state[0]);
})();

// Same, with an object result: valueOf must run exactly once and its
// exception must propagate.
(function TestEagerDeoptPendingObject() {
  const results = [0, {}];
  function cmp(a, b) {
    state[0]++;
    return state[0] === 1 ? results[0] : a - b;
  }
  %PrepareFunctionForOptimization(cmp);
  function f() { return [3, 1, 2].sort(cmp); }
  opt(f);

  class ValueOfError extends Error {}
  results[0] = {
    valueOf() {
      state[1]++;
      throw new ValueOfError();
    }
  };
  state[0] = 0;
  state[1] = 0;
  assertThrows(f, ValueOfError);
  assertEquals(1, state[0]);
  assertEquals(1, state[1]);
})();
