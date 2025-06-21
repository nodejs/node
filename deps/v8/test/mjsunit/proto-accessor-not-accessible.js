// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Accessors for __proto__ are defined in Object.prototype (spec:
// https://tc39.es/ecma262/#sec-object.prototype.__proto__ ). If
// Object.prototype is not in the prototype chain of an object, the accessors
// are not accessible. In particular, __proto__ is treated as a normal property
// and the special meaning (that getting __proto__ would return the prototype
// and setting __proto__ would change the prototype) is lost.

function testObjectWithNullProto(object) {
  assertNull(Object.getPrototypeOf(object));

  // The __proto__ getter is not accessible.
  assertEquals(undefined, object.__proto__);

  // The __proto__ setter is not accessible. Setting __proto__ will create a
  // normal property called __proto__ and not change the prototype.
  object.__proto__ = {};
  assertNull(Object.getPrototypeOf(object));

  // Object.setPrototypeOf can still be used for really setting the prototype.
  const proto1 = {};
  Object.setPrototypeOf(object, proto1);

  // Now the accessors are accessible again.
  assertEquals(proto1, object.__proto__);

  const proto2 = {};
  object.__proto__ = proto2;
  assertEquals(proto2, object.__proto__);
}

(function TestObjectCreatedWithObjectCreate() {
  testObjectWithNullProto(Object.create(null));
})();

(function TestProtoSetToNullAfterCreation() {
  let object_with_null_proto = {};
  object_with_null_proto.__proto__ = null;
  testObjectWithNullProto(Object.create(null));
})();
