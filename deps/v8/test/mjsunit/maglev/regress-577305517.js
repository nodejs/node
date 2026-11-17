// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

([].values()).foo = 1;
const smi_obj = {p: 42};

function nop() {}
%NeverOptimizeFunction(nop);

// Test early abort right after MaglevSubGraphBuilder::BeginLoop in
// Array.prototype.sort.
(function testSort() {
  const sort = Array.prototype.sort;
  function cmp(a, b) {
    return a - b;
  }
  function inner(iter, arr, skip) {
    if (skip) return 0;
    let v = iter.p;
    return sort.call(arr, cmp);
  }
  function test(arr, skip) {
    let iter = arr.values();
    nop();
    let elem = arr[0];
    return inner(iter, arr, skip);
  }

  %PrepareFunctionForOptimization(cmp);
  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(test);
  for (let i = 0; i < 10; ++i) {
    inner(smi_obj, [2, 1], false);
    test([2, 1], true);
  }
  %OptimizeMaglevOnNextCall(test);
  assertEquals(0, test([2, 1], true));
  assertEquals([1, 2], test([2, 1], false));
})();

// Test the same early abort in Array.prototype.forEach
// (TryReduceArrayIteratingBuiltin).
(function testForEach() {
  const forEach = Array.prototype.forEach;
  let sum = 0;
  function cb(x) {
    sum += x;
  }
  function inner(iter, arr, skip) {
    if (skip) return 0;
    let v = iter.p;
    return forEach.call(arr, cb);
  }
  function test(arr, skip) {
    let iter = arr.values();
    nop();
    let elem = arr[0];
    return inner(iter, arr, skip);
  }

  %PrepareFunctionForOptimization(cb);
  %PrepareFunctionForOptimization(inner);
  %PrepareFunctionForOptimization(test);
  for (let i = 0; i < 10; ++i) {
    inner(smi_obj, [2, 1], false);
    test([2, 1], true);
  }
  sum = 0;
  %OptimizeMaglevOnNextCall(test);
  assertEquals(0, test([2, 1], true));
  assertEquals(undefined, test([2, 1], false));
  assertEquals(3, sum);
})();
