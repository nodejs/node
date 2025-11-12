// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --expose-gc

// Based on Mozilla Array.of() tests at http://dxr.mozilla.org/mozilla-central/source/js/src/jit-test/tests/collections

'use strict';

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array];


function TestTypedArrayOf(constructor) {
  // %TypedArray%.of basics.
  var a = constructor.of();
  assertEquals(0, a.length);
  assertEquals(constructor.prototype, Object.getPrototypeOf(a));
  assertEquals(false, Array.isArray(a));

  // Items are coerced to numerical values.
  a = constructor.of(undefined, null, [], true, false, 3.14);

  // For typed arrays of floating point values, values are not rounded.
  if (constructor === Float32Array || constructor === Float64Array) {
    assertEquals(NaN, a[0]);
    assertEquals(0, a[1]);
    assertEquals(0, a[2]);
    assertEquals(1, a[3]);
    assertEquals(0, a[4]);
    assertEquals(true, Math.abs(a[5] - 3.14) < 1e-6);
  } else {
    assertEquals(0, a[0]);
    assertEquals(0, a[1]);
    assertEquals(0, a[2]);
    assertEquals(1, a[3]);
    assertEquals(0, a[4]);
    assertEquals(3, a[5]);
  }

  var aux = [];
  for (var i = 0; i < 100; i++)
    aux[i] = i;

  a = constructor.of.apply(constructor, aux);
  assertEquals(aux.length, a.length);
  assertArrayEquals(aux, a);

  // %TypedArray%.of can be called on subclasses of TypedArrays
  var hits = 0;
  class Bag extends constructor {
    constructor(length) {
      super(length);
      assertEquals(arguments.length, 1);
      assertEquals(length, 2);
      hits++;
    }
  }

  hits = 0;
  a = Bag.of(5, 6);
  assertEquals(1, hits);
  assertEquals(2, a.length);
  assertArrayEquals([5, 6], a);
  assertEquals(Bag.prototype, a.__proto__);

  hits = 0;
  var actual = constructor.of.call(Bag, 5, 6);
  assertEquals(1, hits);
  assertEquals(2, a.length);
  assertArrayEquals([5, 6], a);
  assertEquals(Bag.prototype, a.__proto__);

  // %TypedArray%.of does not trigger prototype setters.
  // (It defines elements rather than assigning to them.)
  var status = "pass";
  Object.defineProperty(constructor.prototype, "0", {
    set: function(v) { status = "fail"; }
  });
  assertEquals(1, constructor.of(1)[0], 1);
  assertEquals("pass", status);

  // Note that %TypedArray%.of does not trigger "length" setter itself, as
  // it relies on the constructor to set "length" to the value passed to it.
  // If the constructor does not assign "length", the setter should not be
  // invoked.

  // Setter on the newly created object.
  class Pack extends constructor {
    constructor(length) {
      super(length);
      Object.defineProperty(this, "length", {
        set: function (v) { status = "fail"; }
      });
    }
  }
  var pack = Pack.of(5, 6, 7, 8);
  assertEquals("pass", status);

  // when the setter is on the new object's prototype
  class Bevy extends constructor {}
  Object.defineProperty(Bevy.prototype, "length", {
    set: function (v) { status = "fail"; }
  });
  var bevy = Bevy.of(3);
  assertEquals("pass", status);

  // Check superficial features of %TypedArray%.of.
  var desc = Object.getOwnPropertyDescriptor(constructor.__proto__, "of");

  assertEquals(desc.configurable, true);
  assertEquals(desc.enumerable, false);
  assertEquals(desc.writable, true);
  assertEquals(constructor.of.length, 0);

  // %TypedArray%.of is not a constructor.
  assertThrows(function() { new constructor.of(); }, TypeError);

  // For receivers which are not constructors %TypedArray%.of does not
  // allocate a typed array using a default constructor, but throws an
  // exception. Note that this is different from Array.of, which uses
  // Array as default constructor.
  for (var x of [undefined, null, false, true, "cow", 42, 3.14]) {
    assertThrows(function () { constructor.of.call(x); }, TypeError);
  }

  // Check if it's correctly accessing new typed array elements even after
  // garbage collection is invoked in ToNumber.
  var not_number = {
    [Symbol.toPrimitive]() {
      gc();
      return 123;
    }
  };
  var dangerous_array = new Array(64).fill(not_number);
  var a = constructor.of(...dangerous_array);
  for (var i = 0; i < 64; i++) {
    assertEquals(123, a[i]);
  }
}

for (var constructor of typedArrayConstructors) {
  TestTypedArrayOf(constructor);
}
