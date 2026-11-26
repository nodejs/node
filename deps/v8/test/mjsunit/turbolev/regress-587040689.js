// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Test 1: Loop-header phi used directly as a cached property receiver key.
(function testPropertyKeyRemap() {
  function f(array, replacement, flag, count, shrinkTo) {
    const initialLength = array.length;
    let current = array;
    let firstIteration = 'first';
    let currentLength;
    let observedLength;
    while (typeof flag === 'number') {
      currentLength = array.length;
      if (typeof firstIteration === 'number') {
        array.length = shrinkTo;
        current.length = initialLength;
      }
      observedLength = current.length;
      current = replacement;
      firstIteration = 1;
      flag = 1;
      if (!--count) return currentLength + observedLength * 0;
    }
    return current;
  }

  const makeArray = () => new Array(100).fill(42);
  const replacement = {length: 20};
  replacement.length = 21;
  %PrepareFunctionForOptimization(f);
  assertEquals(100, f(makeArray(), replacement, 'skip', 4, 10).length);
  for (let i = 0; i < 10; ++i) {
    assertEquals(10, f(makeArray(), replacement, 0, 4, 10));
  }
  const array = makeArray();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f(array, replacement, 0, 4, 10));
  assertEquals(10, array.length);
  assertEquals(100, replacement.length);
})();

// Test 2: Loop-header phi wrapped in an Identity node and normalized by the
// inliner's optimizer pass (triggered via constant-function return) before
// loop peeling.
(function testInlinedIdentityKeyRemap() {
  function id(x) {
    return x;
  }
  function makeId() {
    return id;
  }
  %PrepareFunctionForOptimization(id);
  %PrepareFunctionForOptimization(makeId);
  %DisableEagerInlining(id);
  %DisableEagerInlining(makeId);

  function f(array, replacement, flag, count, shrinkTo) {
    const initialLength = array.length;
    let current = array;
    let firstIteration = 'first';
    let currentLength;
    let observedLength;
    while (typeof flag === 'number') {
      const fn = makeId();
      const receiver = fn(current);
      currentLength = array.length;
      if (typeof firstIteration === 'number') {
        array.length = shrinkTo;
        receiver.length = initialLength;
      }
      observedLength = receiver.length;
      current = replacement;
      firstIteration = 1;
      flag = 1;
      if (!--count) return currentLength + observedLength * 0;
    }
    return current;
  }

  const makeArray = () => new Array(100).fill(42);
  const replacement = {length: 20};
  replacement.length = 21;
  %PrepareFunctionForOptimization(f);
  assertEquals(100, f(makeArray(), replacement, 'skip', 4, 10).length);
  for (let i = 0; i < 10; ++i) {
    assertEquals(10, f(makeArray(), replacement, 0, 4, 10));
  }
  const array = makeArray();
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f(array, replacement, 0, 4, 10));
  assertEquals(10, array.length);
  assertEquals(100, replacement.length);
})();

// Test 3: Two swapped loop-header phis and a plain in-object property.
(function testSwappedHeaderPhisPropertyRemap() {
  function f(a, b, flag, count, newVal) {
    const initial = a.x;
    let p = a;
    let q = b;
    let first = 'first';
    let seen = 0;
    let observed = 0;
    while (typeof flag === 'number') {
      seen = a.x;
      if (typeof first === 'number') {
        a.x = newVal;
        p.x = initial;
      }
      observed = p.x;
      const t = p;
      p = q;
      q = t;
      first = 1;
      flag = 1;
      if (!--count) return seen * 1000 + a.x + observed * 0;
    }
    return 0;
  }

  const makeObj = v => ({x: v, y: 1});
  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(makeObj(100), makeObj(20), 'skip', 5, 10));
  for (let i = 0; i < 10; ++i) {
    assertEquals(10100, f(makeObj(100), makeObj(20), 0, 5, 10));
  }
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10100, f(makeObj(100), makeObj(20), 0, 5, 10));
  assertEquals(100010, f(makeObj(100), makeObj(20), 0, 4, 10));
})();
