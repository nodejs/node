// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

'use strict';

(function TestPrimitivesUsingAtomics() {
  // All primitives can be stored in fields.
  const prims = [42, -0, undefined, null, true, false, 'foo'];
  let arr = new SharedArray(1);
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

  let shared_rhs2 = new SharedArray(10);
  assertEquals(Atomics.exchange(arr, 0, shared_rhs2), shared_rhs);
  assertEquals(arr[0], shared_rhs2);
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
