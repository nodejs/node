// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct --allow-natives-syntax

"use strict";

let S = new SharedStructType(['field', '2']);

(function TestPrimitivesUsingAtomics() {
  // All primitives can be stored in fields.
  let s = new S();
  const prims = [42, -0, undefined, null, true, false, "foo"];

  for (let prim of prims) {
    Atomics.store(s, 'field', prim);
    Atomics.store(s, '2', prim);
    assertEquals(Atomics.load(s, 'field'), prim);
    assertEquals(Atomics.load(s, '2'), prim);
  }

  for (let prim1 of prims) {
    for (let prim2 of prims) {
      s.field = prim1;
      s[2] = prim1;
      assertEquals(Atomics.exchange(s, 'field', prim2), prim1);
      assertEquals(s.field, prim2);
      assertEquals(Atomics.exchange(s, '2', prim2), prim1);
      assertEquals(s[2], prim2);
    }
  }

  const primsCopy = [42, -0, undefined, null, true, false, 'foo'];
  for (let i = 0; i < prims.length; i++) {
    const prim = prims[i];
    const primCopy = primsCopy[i];
    s.field = prim;
    // Atomics.compareExchange mimics `===` comparison.
    assertEquals(Atomics.compareExchange(s, 'field', '42', 'exchanged'), prim);
    assertEquals(s.field, prim);
    assertEquals(
        Atomics.compareExchange(s, 'field', primCopy, 'exchanged'), prim);
    assertEquals(s.field, 'exchanged');

    s[2] = prim;
    assertEquals(Atomics.compareExchange(s, 2, '42', 'exchanged'), prim);
    assertEquals(s[2], prim);
    assertEquals(Atomics.compareExchange(s, 2, primCopy, 'exchanged'), prim);
    assertEquals(s[2], 'exchanged');
  }
})();

(function TestObjectsUsingAtomics() {
  let s = new S();
  // Shared objects cannot point to non-shared objects.
  assertThrows(() => { Atomics.store(s, 'field', []); });
  assertThrows(() => { Atomics.store(s, 'field', {}); });
  // Shared objects can point to other shared objects.

  let shared_rhs = new S();
  Atomics.store(s, 'field', shared_rhs);
  assertEquals(Atomics.load(s, 'field'), shared_rhs);

  shared_rhs = new SharedArray(10);
  Atomics.store(s, 'field', shared_rhs);
  assertEquals(Atomics.load(s, 'field'), shared_rhs);

  let shared_rhs2 = new S();
  assertEquals(Atomics.exchange(s, 'field', shared_rhs2), shared_rhs);
  assertEquals(s.field, shared_rhs2);

  assertEquals(Atomics.exchange(s, 'field', shared_rhs), shared_rhs2);
  assertEquals(s.field, shared_rhs);

  assertEquals(
      Atomics.compareExchange(s, 'field', new SharedArray(10), 'exchanged'),
      shared_rhs);
  assertNotEquals(s.field, 'exchanged');

  assertEquals(
      Atomics.compareExchange(s, 'field', shared_rhs, shared_rhs2), shared_rhs);
  assertEquals(s.field, shared_rhs2);

  assertEquals(
      Atomics.compareExchange(s, 'field', new S(), 'exchanged'), shared_rhs2);
  assertNotEquals(s.field, 'exchanged');

  assertEquals(
      Atomics.compareExchange(s, 'field', shared_rhs2, shared_rhs),
      shared_rhs2);
  assertEquals(s.field, shared_rhs);

})();

(function TestNotExtensibleUsingAtomics() {
  let s = new S();
  // Shared structs are non-extensible.
  assertThrows(() => { Atomics.store(s, 'nonExistent', 42); });
})();

(function TestCompareExchangeNumbers() {
  let fieldNames = new SharedArray(999);
  fieldNames[0] = '2';
  for (let i = 1; i < fieldNames.length; i++) {
    fieldNames[i] = `field${i}`;
  }

  let ST = new SharedStructType(fieldNames)
  let s = new ST();

  // Test in-object fields.
  // We can compare a Smi to a HeapNumber.
  s.field1 = 0;
  assertTrue(%IsSmi(s.field1));
  let HeapNumber = %AllocateHeapNumber();
  assertEquals(
      Atomics.compareExchange(s, 'field1', HeapNumber, 'success'), HeapNumber);
  assertEquals(s.field1, 'success');

  // We can compare the same value stored in 2 different HeapNumbers.
  s.field1 = 1.5;
  HeapNumber = 1.5;
  assertFalse(%IsSameHeapObject(s.field, HeapNumber));
  assertEquals(
      Atomics.compareExchange(s, 'field1', HeapNumber, 'success'), 1.5);
  assertEquals(s.field1, 'success');

  // Test dictionary element fields.
  // We can compare a Smi to a HeapNumber.
  s[2] = 0;
  assertTrue(%IsSmi(s[2]));
  HeapNumber = %AllocateHeapNumber();
  assertEquals(
      Atomics.compareExchange(s, 2, HeapNumber, 'success'), HeapNumber);
  assertEquals(s[2], 'success');

  // We can compare the same value stored in 2 different HeapNumbers.
  s[2] = 1.5;
  HeapNumber = 1.5;
  assertFalse(%IsSameHeapObject(s[2], HeapNumber));
  assertEquals(Atomics.compareExchange(s, 2, HeapNumber, 'success'), 1.5);
  assertEquals(s[2], 'success');

  // Test out-of-object fields.
  // We can compare a Smi to a HeapNumber.
  s.field998 = 0;
  assertTrue(%IsSmi(s.field998));
  HeapNumber = %AllocateHeapNumber();
  assertEquals(
      Atomics.compareExchange(s, 'field998', HeapNumber, 'success'),
      HeapNumber);
  assertEquals(s.field998, 'success');

  // We can compare the same value stored in 2 different HeapNumbers.
  s.field998 = 1.5;
  HeapNumber = 1.5;
  assertFalse(%IsSameHeapObject(s.field, HeapNumber));
  assertEquals(
      Atomics.compareExchange(s, 'field998', HeapNumber, 'success'), 1.5);
  assertEquals(s.field998, 'success');
})();
