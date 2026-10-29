// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-lazy-feedback-allocation

function TestSetWithCustomIterator(ctor) {
  const k1 = {};
  const k2 = {};
  let callCount = 0;
  Array.prototype[Symbol.iterator] = () => ({
    next: () =>
      callCount++ === 0
        ? { value: k2, done: false }
        : { done: true }
  });
  const entries = [k1];
  const set = new ctor(entries);
  assertFalse(set.has(k1));
  assertTrue(set.has(k2));
  assertEquals(2, callCount);
}
%PrepareFunctionForOptimization(TestSetWithCustomIterator);
TestSetWithCustomIterator(Set);
TestSetWithCustomIterator(Set);
TestSetWithCustomIterator(Set);
%OptimizeFunctionOnNextCall(TestSetWithCustomIterator);
TestSetWithCustomIterator(Set);
assertOptimized(TestSetWithCustomIterator);

TestSetWithCustomIterator(WeakSet);
%PrepareFunctionForOptimization(TestSetWithCustomIterator);
TestSetWithCustomIterator(WeakSet);
TestSetWithCustomIterator(WeakSet);
%OptimizeFunctionOnNextCall(TestSetWithCustomIterator);
TestSetWithCustomIterator(WeakSet);
assertOptimized(TestSetWithCustomIterator);

function TestMapWithCustomIterator(ctor) {
  const k1 = {};
  const k2 = {};
  let callCount = 0;
  Array.prototype[Symbol.iterator] = () => ({
    next: () =>
      callCount++ === 0
        ? { value: [k2, 2], done: false }
        : { done: true }
  });
  const entries = [[k1, 1]];
  const map = new ctor(entries);
  assertFalse(map.has(k1));
  assertEquals(2, map.get(k2));
  assertEquals(2, callCount);
}
%PrepareFunctionForOptimization(TestMapWithCustomIterator);
TestMapWithCustomIterator(Map);
TestMapWithCustomIterator(Map);
TestMapWithCustomIterator(Map);
%OptimizeFunctionOnNextCall(TestMapWithCustomIterator);
TestMapWithCustomIterator(Map);
assertOptimized(TestMapWithCustomIterator);

TestMapWithCustomIterator(WeakMap);
%PrepareFunctionForOptimization(TestMapWithCustomIterator);
TestMapWithCustomIterator(WeakMap);
TestMapWithCustomIterator(WeakMap);
%OptimizeFunctionOnNextCall(TestMapWithCustomIterator);
TestMapWithCustomIterator(WeakMap);
assertOptimized(TestMapWithCustomIterator);
