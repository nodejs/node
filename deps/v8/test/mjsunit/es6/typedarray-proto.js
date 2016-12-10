// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that the methods for different TypedArray types have the same
// identity.

'use strict';

let typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];

let TypedArray = Uint8Array.__proto__;
let TypedArrayPrototype = TypedArray.prototype;

assertEquals(TypedArray.__proto__, Function.prototype);
assertEquals(TypedArrayPrototype.__proto__, Object.prototype);

// There are extra own class properties due to it simply being a function
let classProperties = new Set([
  "length", "name", "arguments", "caller", "prototype", "BYTES_PER_ELEMENT"
]);
let instanceProperties = new Set(["BYTES_PER_ELEMENT", "constructor", "prototype"]);

function functionProperties(object) {
  return Object.getOwnPropertyNames(object).filter(function(name) {
    return typeof Object.getOwnPropertyDescriptor(object, name).value
        == "function"
      && name != 'constructor' && name != 'subarray';
  });
}

let typedArrayMethods = functionProperties(Uint8Array.prototype);
let typedArrayClassMethods = functionProperties(Uint8Array);

for (let constructor of typedArrayConstructors) {
  for (let property of Object.getOwnPropertyNames(constructor.prototype)) {
    assertTrue(instanceProperties.has(property), property);
  }
  for (let property of Object.getOwnPropertyNames(constructor)) {
    assertTrue(classProperties.has(property), property);
  }
}

// Abstract %TypedArray% class can't be constructed directly

assertThrows(() => new TypedArray(), TypeError);

// The "prototype" property is nonconfigurable, nonenumerable, nonwritable,
// both for %TypedArray% and for all subclasses

let desc = Object.getOwnPropertyDescriptor(TypedArray, "prototype");
assertFalse(desc.writable);
assertFalse(desc.configurable);
assertFalse(desc.enumerable);

for (let constructor of typedArrayConstructors) {
  let desc = Object.getOwnPropertyDescriptor(constructor, "prototype");
  assertFalse(desc.writable);
  assertFalse(desc.configurable);
  assertFalse(desc.enumerable);
}
