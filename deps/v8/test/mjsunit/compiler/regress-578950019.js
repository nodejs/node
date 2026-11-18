// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

(function TestMaterializedFixedArrayLeftTrim() {
  function inspect(trigger) {
    if (!trigger) return;
    // Materialize the scalar-replaced JSArray and its FixedArray backing store
    // via Function.prototype.arguments, which saves materialized objects in
    // MaterializedObjectStore and schedules a lazy deopt of `foo`.
    const materialized_array = bar.arguments[0];
    // Left-trim the materialized FixedArray in place (requires new_length >
    // JSArray::kMaxCopyElements = 100, so initial length >= 102).
    assertEquals(3, materialized_array.shift());
    assertEquals(101, materialized_array.length);
    // Trigger GC before `foo` unwinds so the marker visits
    // MaterializedObjectStore.
    gc();
    assertSame(materialized_array, bar.arguments[0]);
  }

  function bar(arr, trigger) {
    inspect(trigger);
  }

  function foo(x, trigger) {
    const virtual_packed_array = new Array(
        x, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, x);
    bar(virtual_packed_array, trigger);
  }

  %NeverOptimizeFunction(inspect);
  %PrepareFunctionForOptimization(bar);
  %PrepareFunctionForOptimization(foo);
  foo(1, false);
  foo(2, false);
  %OptimizeFunctionOnNextCall(foo);
  foo(3, true);
})();

(function TestMaterializedFixedDoubleArrayLeftTrim() {
  function inspect(trigger) {
    if (!trigger) return;
    // Materialize the scalar-replaced JSArray and its FixedDoubleArray backing
    // store (elided by Maglev) via Function.prototype.arguments.
    const materialized_double_array = bar.arguments[0];
    assertEquals(3.5, materialized_double_array.shift());
    assertEquals(101, materialized_double_array.length);
    gc();
    assertSame(materialized_double_array, bar.arguments[0]);
  }

  function bar(arr, trigger) {
    inspect(trigger);
  }

  function foo(x, trigger) {
    const virtual_double_array = new Array(
        x, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1, 1.1,
        1.1, 1.1, x);
    bar(virtual_double_array, trigger);
  }

  %NeverOptimizeFunction(inspect);
  %PrepareFunctionForOptimization(bar);
  %PrepareFunctionForOptimization(foo);
  foo(1.5, false);
  foo(2.5, false);
  %OptimizeFunctionOnNextCall(foo);
  foo(3.5, true);
})();
