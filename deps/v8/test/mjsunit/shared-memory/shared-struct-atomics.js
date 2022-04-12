// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

"use strict";

let S = new SharedStructType(['field']);

(function TestPrimitivesUsingAtomics() {
  // All primitives can be stored in fields.
  let s = new S();
  for (let prim of [42, -0, undefined, null, true, false, "foo"]) {
    Atomics.store(s, 'field', prim);
    assertEquals(Atomics.load(s, 'field'), prim);
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
})();

(function TestNotExtensibleUsingAtomics() {
  let s = new S();
  // Shared structs are non-extensible.
  assertThrows(() => { Atomics.store(s, 'nonExistent', 42); });
})();
