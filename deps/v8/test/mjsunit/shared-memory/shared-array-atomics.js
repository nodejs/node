// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct --allow-natives-syntax

'use strict';

(function TestPrimitivesUsingAtomics() {
  // All primitives can be stored in fields.
  const prims = [42, -0, undefined, null, true, false, 'foo'];
  const arr = new SharedArray(1);
  for (let prim of prims) {
    Atomics.store(arr, 0, prim);
    assertEquals(Atomics.load(arr, 0), prim);
  }

  for (let prim1 of prims) {
    for (let prim2 of prims) {
      arr[0] = prim1;
      assertEquals(Atomics.exchange(arr, 0, prim2), prim1);
      assertEquals(arr[0], prim2);
    }
  }

  const primsCopy = [42, -0, undefined, null, true, false, 'foo'];
  for (let i = 0; i < prims.length; i++) {
    const prim = prims[i];
    const primCopy = primsCopy[i];
    arr[0] = prim;
    // Atomics.compareExchange mimics `===` comparison.
    assertEquals(Atomics.compareExchange(arr, 0, '42', 'exchanged'), prim);
    assertEquals(arr[0], prim);
    assertEquals(Atomics.compareExchange(arr, 0, primCopy, 'exchanged'), prim);
    assertEquals(arr[0], 'exchanged');
  }
})();

(function TestObjectsUsingAtomics() {
  let arr = new SharedArray(1);
  // Shared objects cannot point to non-shared objects.
  assertThrows(() => {
    Atomics.store(arr, 0, []);
  });
  assertThrows(() => {
    Atomics.store(arr, 0, {});
  });
  // Shared objects can point to other shared objects.
  let shared_rhs = new SharedArray(10);
  Atomics.store(arr, 0, shared_rhs);
  assertEquals(Atomics.load(arr, 0), shared_rhs);

  let Struct = new SharedStructType(['field']);
  shared_rhs = new Struct();
  Atomics.store(arr, 0, shared_rhs);
  assertEquals(Atomics.load(arr, 0), shared_rhs);

  // Test we can exchange shared objects.
  let shared_rhs2 = new SharedArray(10);
  assertEquals(Atomics.exchange(arr, 0, shared_rhs2), shared_rhs);
  assertEquals(arr[0], shared_rhs2);

  assertEquals(Atomics.exchange(arr, 0, shared_rhs), shared_rhs2);
  assertEquals(arr[0], shared_rhs);

  // Verify we can compare-exchange shared objects.

  // Comparing to a different instance of the same type is not succesful.
  assertEquals(
      Atomics.compareExchange(arr, 0, new Struct(), 'exchanged'), shared_rhs);
  assertNotEquals(arr[0], 'exchanged');

  assertEquals(
      Atomics.compareExchange(arr, 0, shared_rhs, shared_rhs2), shared_rhs);
  assertEquals(arr[0], shared_rhs2);

  // Comparing to a different instance of the same type is not succesful.
  assertEquals(
      Atomics.compareExchange(arr, 0, new SharedArray(10), 'exchanged'),
      shared_rhs2);
  assertNotEquals(arr[0], 'exchanged');

  assertEquals(
      Atomics.compareExchange(arr, 0, shared_rhs2, shared_rhs), shared_rhs2);
  assertEquals(arr[0], shared_rhs);
})();

(function TestCompareExchangeNumbers() {
  const arr = new SharedArray(1);
  // We can compare a Smi to a HeapNumber.
  arr[0] = 0;
  assertTrue(%IsSmi(arr[0]));
  let HeapNumber = %AllocateHeapNumber();
  assertEquals(
      Atomics.compareExchange(arr, 0, HeapNumber, 'exchanged'), HeapNumber);
  assertEquals(arr[0], 'exchanged');

  // We can compare the same value stored in 2 different HeapNumbers;
  arr[0] = 1.5;
  HeapNumber = 1.5;
  assertFalse(%IsSameHeapObject(arr[0], HeapNumber));
  assertEquals(Atomics.compareExchange(arr, 0, HeapNumber, 'exchanged'), 1.5);
  assertEquals(arr[0], 'exchanged');
})();

(function TestOutOfBounds() {
  let arr = new SharedArray(1);
  // Shared structs are non-extensible.
  assertThrows(() => {
    Atomics.store(arr, 2, 42);
  });
  assertThrows(() => {
    Atomics.store(arr, -1, 42);
  });
  assertThrows(() => {
    Atomics.store(arr, 'field', 42);
  });
})();

(function TestLengthReadOnly() {
  let arr = new SharedArray(2);
  assertEquals(2, Atomics.load(arr, 'length'));
  // The 'length' property is read-only. Atomics.store and Atomics.exchange are
  // treated like strict functions, and so an error is always thrown.
  assertThrows(() => {
    Atomics.store(arr, 'length', 42);
  });
  assertThrows(() => {
    Atomics.exchange(arr, 'length', 42);
  });
})();
