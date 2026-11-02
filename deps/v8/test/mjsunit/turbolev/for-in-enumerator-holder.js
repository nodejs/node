// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

(function() {
  function firstKey(o) {
    for (const k in o) { return k; }
    return 'empty';
  }
  %PrepareFunctionForOptimization(firstKey);

  const arr = [1, 2, 3];
  assertEquals("0", firstKey(arr));
  assertEquals("0", firstKey(arr));
  %OptimizeFunctionOnNextCall(firstKey);
  assertEquals("0", firstKey(arr));
  assertOptimized(firstKey);
})();

(function() {
  function allKeys(o) {
    // Initialize as PACKED_ELEMENTS to avoid deopts when allocation site
    // tracking is disabled (e.g. single_generation).
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const arr = [1, 2];
  arr.foo = 'bar';
  arr.bar = 'baz';

  assertEquals(["0", "1", "foo", "bar"], allKeys(arr));
  assertEquals(["0", "1", "foo", "bar"], allKeys(arr));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "1", "foo", "bar"], allKeys(arr));
  assertOptimized(allKeys);
})();

(function() {
  function iterateAndMutate(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') {
        o.new_prop = 'new';
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndMutate);

  const arr = [1, 2];
  arr.foo = 'bar';

  // Warmup with fresh objects to avoid polluting the map history too much,
  // but they must have the same shape.
  for (let i = 0; i < 20; i++) {
    const temp = [1, 2];
    temp.foo = 'bar';
    iterateAndMutate(temp);
  }

  %OptimizeFunctionOnNextCall(iterateAndMutate);
  const my_arr = [1, 2];
  my_arr.foo = 'bar';
  assertEquals(["0", "1", "foo"], iterateAndMutate(my_arr));
})();

// Shared DescriptorArray / EnumCache where a child map has more properties
// than the parent map: must use the receiver map's EnumLength, not the full
// EnumCache keys array length.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const a = [1];
  a.x = 10;
  a.y = 20;
  for (let k in a) {}

  const b = [1];
  b.x = 10;
  assertEquals(["0", "x"], allKeys(b));
  assertEquals(["0", "x"], allKeys(b));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "x"], allKeys(b));
  assertOptimized(allKeys);
})();

// JSArray with slack capacity (elements.length > array.length): must only
// enumerate up to array.length, not elements.length.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const a = [];
  a.push(10, 20);
  assertEquals(["0", "1"], allKeys(a));
  assertEquals(["0", "1"], allKeys(a));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(["0", "1"], allKeys(a));
  assertOptimized(allKeys);
})();

// OSR inside a for..in loop over a large array on its very first execution
// (feedback starts uninitialized, then transitions to kEnumeratorHolder).
(function() {
  const big = [];
  for (let i = 0; i < 5000; i++) big.push(i);
  big.extra = 'end';

  function osrLoop(o) {
    let count = 0;
    let last = '';
    for (const k in o) {
      count++;
      last = k;
    }
    return [count, last];
  }
  assertEquals([5001, 'extra'], osrLoop(big));
})();

// Nested loops with for..in over packed array with named properties.
(function() {
  function peeledForIn(o) {
    const keys = [''];
    keys.length = 0;
    for (let i = 0; i < 3; i++) {
      for (const k in o) {
        keys.push(k);
        for (let j = 0; j < 2; j++) {
          if (j === 1 && k === 'a') keys.push('inner');
        }
      }
    }
    return keys;
  }
  %PrepareFunctionForOptimization(peeledForIn);

  const arr = [1, 2];
  arr.a = 10;
  const expected = [
    '0', '1', 'a', 'inner',
    '0', '1', 'a', 'inner',
    '0', '1', 'a', 'inner',
  ];
  assertEquals(expected, peeledForIn(arr));
  assertEquals(expected, peeledForIn(arr));
  %OptimizeFunctionOnNextCall(peeledForIn);
  assertEquals(expected, peeledForIn(arr));
  assertOptimized(peeledForIn);
})();

// Non-JSArray with packed elements (object literal and strict arguments),
// including adding an indexed element mid-loop.
(function() {
  function allKeys(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) { keys.push(k); }
    return keys;
  }
  %PrepareFunctionForOptimization(allKeys);

  const obj = {0: 'a', 1: 'b', foo: 'bar'};
  const args = (function() { 'use strict'; return arguments; })('x', 'y');
  assertEquals(['0', '1', 'foo'], allKeys(obj));
  assertEquals(['0', '1'], allKeys(args));
  %OptimizeFunctionOnNextCall(allKeys);
  assertEquals(['0', '1', 'foo'], allKeys(obj));
  assertEquals(['0', '1'], allKeys(args));
  assertOptimized(allKeys);

  function addIndexedOnObject(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') o[2] = 'c';
    }
    return keys;
  }
  %PrepareFunctionForOptimization(addIndexedOnObject);
  for (let i = 0; i < 10; i++) {
    addIndexedOnObject({0: 'a', 1: 'b', foo: 'bar'});
  }
  %OptimizeFunctionOnNextCall(addIndexedOnObject);
  assertEquals(['0', '1', 'foo'],
               addIndexedOnObject({0: 'a', 1: 'b', foo: 'bar'}));
})();

// JSArray push and pop mid-loop.
(function() {
  function iterateAndPush(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') o.push(99);
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndPush);
  for (let i = 0; i < 10; i++) {
    const a = [1, 2];
    a.foo = 'bar';
    iterateAndPush(a);
  }
  %OptimizeFunctionOnNextCall(iterateAndPush);
  const a1 = [1, 2];
  a1.foo = 'bar';
  assertEquals(['0', '1', 'foo'], iterateAndPush(a1));

  function iterateAndPop(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') o.pop();
    }
    return keys;
  }
  %PrepareFunctionForOptimization(iterateAndPop);
  for (let i = 0; i < 10; i++) {
    const a = [1, 2];
    a.foo = 'bar';
    iterateAndPop(a);
  }
  %OptimizeFunctionOnNextCall(iterateAndPop);
  const a2 = [1, 2];
  a2.foo = 'bar';
  assertEquals(['0', 'foo'], iterateAndPop(a2));
})();

// JSArray elements kind transition mid-loop (PACKED_SMI -> PACKED_DOUBLE and
// PACKED_SMI -> PACKED_ELEMENTS).
(function() {
  function transitionToDouble(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') o[1] = 1.5;
    }
    return keys;
  }
  %PrepareFunctionForOptimization(transitionToDouble);
  for (let i = 0; i < 10; i++) {
    const a = [1, 2];
    a.foo = 'bar';
    transitionToDouble(a);
  }
  %OptimizeFunctionOnNextCall(transitionToDouble);
  const a1 = [1, 2];
  a1.foo = 'bar';
  assertEquals(['0', '1', 'foo'], transitionToDouble(a1));

  function transitionToTagged(o) {
    const keys = [''];
    keys.length = 0;
    for (const k in o) {
      keys.push(k);
      if (k === '0') o[1] = 'str';
    }
    return keys;
  }
  %PrepareFunctionForOptimization(transitionToTagged);
  for (let i = 0; i < 10; i++) {
    const a = [1, 2];
    a.foo = 'bar';
    transitionToTagged(a);
  }
  %OptimizeFunctionOnNextCall(transitionToTagged);
  const a2 = [1, 2];
  a2.foo = 'bar';
  assertEquals(['0', '1', 'foo'], transitionToTagged(a2));
})();
