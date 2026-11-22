// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Tests that when --forin-enumerator-holder is enabled, holey elements kinds safely
// fall back to standard enum cache iteration (FixedArray) and maintain correct ES spec
// enumeration order even when mutated during iteration.

// Test 1: Holey Smi elements.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  // o has hole at index 1.
  const o = [1, , 3];
  assertEquals(["0", "2"], allKeys(o));
  assertEquals(["0", "2"], allKeys(o));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "2"], allKeys(o));
  assertOptimized(allKeys);
})();

// Test 2: Holey Double elements.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const o = [1.1, , 3.3];
  assertEquals(["0", "2"], allKeys(o));
  assertEquals(["0", "2"], allKeys(o));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "2"], allKeys(o));
  assertOptimized(allKeys);
})();

// Test 3: Holey Object elements.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const o = [{a: 1}, , {c: 3}];
  assertEquals(["0", "2"], allKeys(o));
  assertEquals(["0", "2"], allKeys(o));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "2"], allKeys(o));
  assertOptimized(allKeys);
})();

// Test 4: Mutation during iteration - deleting elements.
(function() {
  function iterateAndDelete(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        delete o[1]; // delete next element (which becomes hole)
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndDelete);

  for (let i = 0; i < 20; i++) {
    iterateAndDelete([1, 2, 3]);
  }

  %OptimizeFunctionOnNextCall(iterateAndDelete);
  const arr1 = [1, 2, 3];
  assertEquals(["0", "2"], iterateAndDelete(arr1)); // 1 is deleted, should be skipped.

  // Case 4b: Start with holey, delete element. Map doesn't change.
  function iterateAndDeleteHole(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        delete o[2]; // delete index 2
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndDeleteHole);
  for (let i = 0; i < 20; i++) {
    iterateAndDeleteHole([1, , 3, 4]);
  }
  %OptimizeFunctionOnNextCall(iterateAndDeleteHole);
  const arr2 = [1, , 3, 4];
  assertEquals(["0", "3"], iterateAndDeleteHole(arr2));
  assertOptimized(iterateAndDeleteHole);
})();

// Test 5: Mutation of length during iteration.
(function() {
  function iterateAndShrink(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        o.length = 1; // shrink array
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndShrink);
  for (let i = 0; i < 20; i++) {
    iterateAndShrink([1, 2, 3]);
  }
  %OptimizeFunctionOnNextCall(iterateAndShrink);

  const arr = [1, 2, 3];
  assertEquals(["0"], iterateAndShrink(arr));
})();

// Test 6: In-place hole filling during for..in on a holey array must not
// enumerate the newly added property if the implementation uses snapshot
// EnumCache semantics.
(function() {
  function fillHoleDuringForIn(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        o[1] = 2;
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(fillHoleDuringForIn);

  for (let i = 0; i < 20; i++) {
    fillHoleDuringForIn([1, , 3]);
  }
  %OptimizeFunctionOnNextCall(fillHoleDuringForIn);
  assertEquals(["0", "2"], fillHoleDuringForIn([1, , 3]));
  assertOptimized(fillHoleDuringForIn);
})();

// Test 7: Array.prototype.unshift during for..in on a holey array.
(function() {
  function unshiftDuringForIn(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        o.unshift(0);
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(unshiftDuringForIn);

  for (let i = 0; i < 20; i++) {
    unshiftDuringForIn([1, , 3]);
  }
  %OptimizeFunctionOnNextCall(unshiftDuringForIn);
  assertEquals(["0"], unshiftDuringForIn([1, , 3]));
  assertOptimized(unshiftDuringForIn);
})();
