// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

// A holey-double load consumed only by float arithmetic no longer needs the
// not-hole/undefined check: the hole reads as undefined and `undefined + x` is
// NaN, which float arithmetic produces from any NaN input. So loading a hole
// must NOT deopt; it must stay optimized and yield NaN.

// (1) Behavioral flip: arithmetic-only use stays optimized on a hole.
(function() {
  function f(a, i) { return 1 + a[i]; }
  let arr = [0.5, , 1.5];  // HOLEY_DOUBLE_ELEMENTS
  %PrepareFunctionForOptimization(f);
  assertEquals(1.5, f(arr, 0));
  assertEquals(1.5, f(arr, 0));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1.5, f(arr, 0));
  assertOptimized(f);
  // Loading a hole must NOT trigger a deopt (the check is elided).
  assertEquals(NaN, f(arr, 1));  // 1 + undefined = NaN
  assertOptimized(f);
})();

// (2) The value crosses a `Star` (let-binding) before the add. The decision is
// use-based, not bytecode-adjacency-based, so this still elides the check.
(function() {
  function foo(a, i, j) { let x = a[i]; let y = a[j]; return x + y; }
  let arr = [1.5, , 2, 3];
  %PrepareFunctionForOptimization(foo);
  assertEquals(3.5, foo(arr, 0, 2));
  assertEquals(3.5, foo(arr, 0, 2));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3.5, foo(arr, 0, 2));
  assertOptimized(foo);
  assertEquals(NaN, foo(arr, 1, 0));  // undefined + 1.5 = NaN
  assertOptimized(foo);
})();

// (3) Relational comparison of a hole is still correct (undefined < x is
// NaN < x = false). The check is not elided for relational compares in this
// change, so a hole may deopt; only the value is asserted.
(function() {
  function lt(a, i) { return a[i] < 2.0; }
  let arr = [1.5, , 3.5];
  %PrepareFunctionForOptimization(lt);
  assertTrue(lt(arr, 0));
  assertTrue(lt(arr, 0));
  %OptimizeFunctionOnNextCall(lt);
  assertFalse(lt(arr, 1));  // undefined < 2 is false
})();

// (4) HAZARD: equality is NOT hole-tolerant. `undefined === undefined` is true,
// `NaN === NaN` is false, so the result must be the interpreter's answer.
(function() {
  function eq(a, i, j) { return a[i] === a[j]; }
  let arr = [1.5, , 2.5];
  %PrepareFunctionForOptimization(eq);
  assertTrue(eq(arr, 0, 0));
  assertFalse(eq(arr, 0, 2));
  %OptimizeFunctionOnNextCall(eq);
  assertTrue(eq(arr, 1, 1));   // undefined === undefined
  assertFalse(eq(arr, 0, 1));  // 1.5 === undefined
})();

// (5) HAZARD: property access on a hole must throw (undefined.x), not fold.
(function() {
  function prop(a, i) { return a[i].x; }
  let arr = [1.5, , 2.5];
  %PrepareFunctionForOptimization(prop);
  assertEquals(undefined, prop(arr, 0));  // (1.5).x is undefined
  assertEquals(undefined, prop(arr, 0));
  %OptimizeFunctionOnNextCall(prop);
  assertThrows(() => prop(arr, 1), TypeError);  // undefined.x throws
})();

// (6) HAZARD: storing an arithmetic result back into a holey-double array must
// store a real NaN element (silenced), never a hole marker.
(function() {
  function s(a, out, i) { out[0] = a[i] * 1; }
  let arr = [1.5, , 2.5];
  let out = [7.5, 8.5];
  %PrepareFunctionForOptimization(s);
  s(arr, out, 0);
  s(arr, out, 0);
  %OptimizeFunctionOnNextCall(s);
  s(arr, out, 1);              // out[0] = undefined * 1 = NaN
  assertTrue(0 in out);
  assertEquals(NaN, out[0]);
})();

// (7) `undefined` stored into a holey-double array (undefined-NaN) flows through
// arithmetic as NaN and round-trips as undefined.
(function() {
  function r(a, i) { return a[i] + 0; }
  let arr = [1.5, 2.5, 3.5];
  arr[1] = undefined;  // stays HOLEY_DOUBLE, stores the undefined-NaN pattern
  %PrepareFunctionForOptimization(r);
  assertEquals(1.5, r(arr, 0));
  assertEquals(1.5, r(arr, 0));
  %OptimizeFunctionOnNextCall(r);
  assertEquals(NaN, r(arr, 1));  // undefined + 0 = NaN
  assertOptimized(r);
  assertTrue(1 in arr);
  assertTrue(arr[1] === undefined);
})();
