// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table --harmony-struct

"use strict";

let S = new SharedStructType(['field']);

(function TestNoPrototype() {
  // For now the experimental shared structs don't have a prototype, unlike the
  // proposal explainer which says accessing the prototype throws.
  assertNull(S.prototype);
  assertNull(Object.getPrototypeOf(new S()));
})();

(function TestPrimitives() {
  // All primitives can be stored in fields.
  let s = new S();
  for (let prim of [42, -0, Math.random(),
                    undefined, null, true, false,
                    "foo"]) {
    s.field = prim;
    assertEquals(s.field, prim);
  }
})();

(function TestObjects() {
  let s = new S();
  // Shared objects cannot point to non-shared objects.
  assertThrows(() => { s.field = []; });
  assertThrows(() => { s.field = {}; });
  // Shared objects can point to other shared objects.
  let shared_rhs = new S();
  s.field = shared_rhs;
  assertEquals(s.field, shared_rhs);
})();

(function TestNotExtensible() {
  let s = new S();
  // Shared structs are non-extensible.
  assertThrows(() => { s.nonExistent = 42; });
  assertThrows(() => { Object.setPrototypeOf(s, {}); });
  assertThrows(() => { Object.defineProperty(s, 'nonExistent', { value: 42 }); });
})();

(function TestTooManyFields() {
  let field_names = [];
  for (let i = 0; i < 1000; i++) {
    field_names.push('field' + i);
  }
  assertThrows(() => { new SharedStructType(field_names); });
})();
