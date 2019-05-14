// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt

function TestSetWithModifiedIterator(ctor) {
  const k1 = {};
  const k2 = {};
  const entries = [k1, k2];
  const arrayIteratorProto = Object.getPrototypeOf(entries[Symbol.iterator]());
  const originalNext = arrayIteratorProto.next;
  let callCount = 0;
  arrayIteratorProto.next = function() {
    callCount++;
    return originalNext.call(this);
  };

  const set = new ctor(entries);
  assertEquals(3, callCount); // +1 for iterator done

  if('size' in set) assertEquals(2, set.size);
  assertTrue(set.has(k1));
  assertTrue(set.has(k2));

  arrayIteratorProto.next = originalNext;
}
TestSetWithModifiedIterator(Set);
TestSetWithModifiedIterator(Set);
TestSetWithModifiedIterator(Set);
%OptimizeFunctionOnNextCall(TestSetWithModifiedIterator);
TestSetWithModifiedIterator(Set);
assertOptimized(TestSetWithModifiedIterator);
%DeoptimizeFunction(TestSetWithModifiedIterator);

TestSetWithModifiedIterator(WeakSet);
TestSetWithModifiedIterator(WeakSet);
TestSetWithModifiedIterator(WeakSet);
%OptimizeFunctionOnNextCall(TestSetWithModifiedIterator);
TestSetWithModifiedIterator(WeakSet);
assertOptimized(TestSetWithModifiedIterator);
%DeoptimizeFunction(TestSetWithModifiedIterator);


function TestMapWithModifiedIterator(ctor) {
  const k1 = {};
  const k2 = {};
  const entries = [[k1, 1], [k2, 2]];
  const arrayIteratorProto = Object.getPrototypeOf(entries[Symbol.iterator]());
  const originalNext = arrayIteratorProto.next;
  let callCount = 0;
  arrayIteratorProto.next = function() {
    callCount++;
    return originalNext.call(this);
  };

  const set = new ctor(entries);
  assertEquals(3, callCount); // +1 for iterator done

  if('size' in set) assertEquals(2, set.size);
  assertEquals(1, set.get(k1));
  assertEquals(2, set.get(k2));

  arrayIteratorProto.next = originalNext;
}
TestMapWithModifiedIterator(Map);
TestMapWithModifiedIterator(Map);
TestMapWithModifiedIterator(Map);
%OptimizeFunctionOnNextCall(TestMapWithModifiedIterator);
TestMapWithModifiedIterator(Map);
assertOptimized(TestMapWithModifiedIterator);
%DeoptimizeFunction(TestMapWithModifiedIterator);

TestMapWithModifiedIterator(WeakMap);
TestMapWithModifiedIterator(WeakMap);
TestMapWithModifiedIterator(WeakMap);
%OptimizeFunctionOnNextCall(TestMapWithModifiedIterator);
TestMapWithModifiedIterator(WeakMap);
assertOptimized(TestMapWithModifiedIterator);
%DeoptimizeFunction(TestMapWithModifiedIterator);
