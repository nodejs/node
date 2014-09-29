// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var constructors = [Uint8Array, Int8Array,
                    Uint16Array, Int16Array,
                    Uint32Array, Int32Array,
                    Float32Array, Float64Array,
                    Uint8ClampedArray];

function TestTypedArrayPrototype(constructor) {
  assertTrue(constructor.prototype.hasOwnProperty('entries'));
  assertTrue(constructor.prototype.hasOwnProperty('values'));
  assertTrue(constructor.prototype.hasOwnProperty('keys'));
  assertTrue(constructor.prototype.hasOwnProperty(Symbol.iterator));

  assertFalse(constructor.prototype.propertyIsEnumerable('entries'));
  assertFalse(constructor.prototype.propertyIsEnumerable('values'));
  assertFalse(constructor.prototype.propertyIsEnumerable('keys'));
  assertFalse(constructor.prototype.propertyIsEnumerable(Symbol.iterator));

  assertEquals(Array.prototype.entries, constructor.prototype.entries);
  assertEquals(Array.prototype.values, constructor.prototype.values);
  assertEquals(Array.prototype.keys, constructor.prototype.keys);
  assertEquals(Array.prototype.values, constructor.prototype[Symbol.iterator]);
}
constructors.forEach(TestTypedArrayPrototype);


function TestTypedArrayValues(constructor) {
  var array = [1, 2, 3];
  var i = 0;
  for (var value of new constructor(array)) {
    assertEquals(array[i++], value);
  }
  assertEquals(i, array.length);
}
constructors.forEach(TestTypedArrayValues);
