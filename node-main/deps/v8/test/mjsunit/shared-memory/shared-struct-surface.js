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
  assertThrows(() => {
    S.prototype = {};
  });
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
  shared_rhs = new SharedArray(10);
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
  let fieldNames = [];
  for (let i = 0; i < 1000; i++) {
    fieldNames.push('field' + i);
  }
  assertThrows(() => {
    new SharedStructType(fieldNames);
  });
})();

(function TestOwnPropertyEnumeration() {
  let s = new S();
  s.field = 42;

  assertArrayEquals(['field'], Reflect.ownKeys(s));

  let propDescs = Object.getOwnPropertyDescriptors(s);
  let desc = propDescs['field'];
  assertEquals(true, desc.writable);
  assertEquals(false, desc.configurable);
  assertEquals(true, desc.enumerable);
  assertEquals(42, desc.value);

  let vals = Object.values(s);
  assertArrayEquals([42], vals);

  let entries = Object.entries(s);
  assertEquals(1, entries.length);
  assertArrayEquals(['field', 42], entries[0]);
})();

(function TestForIn() {
  let fieldNames = [];
  for (let i = 0; i < 512; i++) {
    fieldNames.push('field' + i);
  }
  let S2 = new SharedStructType(fieldNames);
  let s = new S2();
  let propNames = [];
  for (let prop in s) propNames.push(prop);
  assertArrayEquals(propNames, fieldNames);
})();

(function TestDuplicateFieldNames() {
  assertThrows(() => new SharedStructType(['same', 'same']));
})();

(function TestNoFields() {
  const EmptyStruct = new SharedStructType([]);
  let s = new EmptyStruct();
})();

(function TestSymbolsDisallowed() {
  // This may be relaxed in the future.
  assertThrows(() => new SharedStructType([Symbol()]));
})();

(function TestUsedAsPrototype() {
  const OnPrototypeStruct = new SharedStructType(['prop']);
  let ps = new OnPrototypeStruct();
  ps.prop = "on proto";

  function assertProtoIsStruct(obj, proto) {
    // __proto__ is on Object.prototype, and obj here no longer has
    // Object.prototype as its [[Prototype]].
    assertSame(undefined, obj.__proto__);
    assertSame(proto, Object.getPrototypeOf(obj));
    assertEquals("on proto", obj.prop);
  }

  {
    let pojo = { __proto__: ps };
    assertProtoIsStruct(pojo, ps);
  }

  {
    let pojo = {};
    Object.setPrototypeOf(pojo, ps);
    assertProtoIsStruct(pojo, ps);
  }

  {
    let pojo = {};
    pojo.__proto__ = ps;
    assertProtoIsStruct(pojo, ps);
  }

  {
    const old = globalThis.__proto__;
    globalThis.__proto__ = ps;
    globalThis.__proto__ = old;
  }

  {
    Object.create(ps);
  }

  {
    function Ctor() {}
    Ctor.prototype = ps;
  }
})();

(function TestElements() {
  const largeIndex = 2 ** 32;
  const anotherLargeIndex = 2 ** 31;
  const StructWithElements =
      new SharedStructType([1, 42, largeIndex, anotherLargeIndex]);
  let s = new StructWithElements();
  s[1] = 0.1;
  s[42] = 42;
  s[largeIndex] = 'whatever';
  s[anotherLargeIndex] = 'whatever2';
  assertEquals(0.1, s[1]);
  assertEquals(42, s[42]);
  assertEquals('whatever', s[largeIndex]);
  assertEquals('whatever2', s[anotherLargeIndex]);
  assertThrows(() => s[80] = 0.2);
})();
