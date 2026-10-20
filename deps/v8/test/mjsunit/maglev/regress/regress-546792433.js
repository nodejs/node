// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

const realm = Realm.createAllowCrossRealmAccess();

{
  const otherValues = Realm.eval(realm, 'Array.prototype.values');
  const otherIteratorPrototype = Realm.eval(
    realm,
    'Object.getPrototypeOf(Array.prototype.values.call([]))'
  );

  function testArrayValues(fn, receiver) {
    return fn.call(receiver);
  }

  %PrepareFunctionForOptimization(testArrayValues);
  testArrayValues(otherValues, [1]);
  testArrayValues(otherValues, [2]);
  %OptimizeMaglevOnNextCall(testArrayValues);
  const it = testArrayValues(otherValues, [3]);
  assertEquals(Object.getPrototypeOf(it), otherIteratorPrototype);

  function testArrayValuesDirect() {
    return otherValues.call([1]);
  }

  %PrepareFunctionForOptimization(testArrayValuesDirect);
  testArrayValuesDirect();
  testArrayValuesDirect();
  %OptimizeMaglevOnNextCall(testArrayValuesDirect);
  const itDirect = testArrayValuesDirect();
  assertEquals(Object.getPrototypeOf(itDirect), otherIteratorPrototype);
}

{
  const otherStringIterator = Realm.eval(
    realm,
    'String.prototype[Symbol.iterator]'
  );
  const otherStringIteratorPrototype = Realm.eval(
    realm,
    'Object.getPrototypeOf(String.prototype[Symbol.iterator].call(""))'
  );

  function testStringIterator(fn, receiver) {
    return fn.call(receiver);
  }

  %PrepareFunctionForOptimization(testStringIterator);
  testStringIterator(otherStringIterator, 'a');
  testStringIterator(otherStringIterator, 'b');
  %OptimizeMaglevOnNextCall(testStringIterator);
  const it = testStringIterator(otherStringIterator, 'c');
  assertEquals(Object.getPrototypeOf(it), otherStringIteratorPrototype);

  function testStringIteratorDirect() {
    return otherStringIterator.call('abc');
  }

  %PrepareFunctionForOptimization(testStringIteratorDirect);
  testStringIteratorDirect();
  testStringIteratorDirect();
  %OptimizeMaglevOnNextCall(testStringIteratorDirect);
  const itDirect = testStringIteratorDirect();
  assertEquals(Object.getPrototypeOf(itDirect), otherStringIteratorPrototype);
}

{
  const otherMapEntries = Realm.eval(realm, 'Map.prototype.entries');
  const otherMapIteratorPrototype = Realm.eval(
    realm,
    'Object.getPrototypeOf(new Map().entries())'
  );
  const map = Realm.eval(realm, 'new Map([[1, 2]])');

  function testMapEntries(fn, receiver) {
    return fn.call(receiver);
  }

  %PrepareFunctionForOptimization(testMapEntries);
  testMapEntries(otherMapEntries, map);
  testMapEntries(otherMapEntries, map);
  %OptimizeMaglevOnNextCall(testMapEntries);
  const it = testMapEntries(otherMapEntries, map);
  assertEquals(Object.getPrototypeOf(it), otherMapIteratorPrototype);

  function testMapEntriesDirect() {
    return otherMapEntries.call(map);
  }

  %PrepareFunctionForOptimization(testMapEntriesDirect);
  testMapEntriesDirect();
  testMapEntriesDirect();
  %OptimizeMaglevOnNextCall(testMapEntriesDirect);
  const itDirect = testMapEntriesDirect();
  assertEquals(Object.getPrototypeOf(itDirect), otherMapIteratorPrototype);
}
