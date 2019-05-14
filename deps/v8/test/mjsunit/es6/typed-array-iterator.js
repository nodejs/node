// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var constructors = [Uint8Array, Int8Array,
                    Uint16Array, Int16Array,
                    Uint32Array, Int32Array,
                    Float32Array, Float64Array,
                    Uint8ClampedArray];

var TypedArrayPrototype = Uint8Array.prototype.__proto__;

assertTrue(TypedArrayPrototype.hasOwnProperty('entries'));
assertTrue(TypedArrayPrototype.hasOwnProperty('values'));
assertTrue(TypedArrayPrototype.hasOwnProperty('keys'));
assertTrue(TypedArrayPrototype.hasOwnProperty(Symbol.iterator));

assertFalse(TypedArrayPrototype.propertyIsEnumerable('entries'));
assertFalse(TypedArrayPrototype.propertyIsEnumerable('values'));
assertFalse(TypedArrayPrototype.propertyIsEnumerable('keys'));
assertFalse(TypedArrayPrototype.propertyIsEnumerable(Symbol.iterator));

assertFalse(Array.prototype.entries === TypedArrayPrototype.entries);
assertFalse(Array.prototype[Symbol.iterator] === TypedArrayPrototype.values);
assertFalse(Array.prototype.keys === TypedArrayPrototype.keys);
assertFalse(Array.prototype[Symbol.iterator] === TypedArrayPrototype[Symbol.iterator]);


function TestTypedArrayValues(constructor) {
  var array = [1, 2, 3];
  var i = 0;
  for (var value of new constructor(array)) {
    assertEquals(array[i++], value);
  }
  assertEquals(i, array.length);
}
constructors.forEach(TestTypedArrayValues);
