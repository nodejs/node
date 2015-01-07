// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on Mozilla Array.of() tests at http://dxr.mozilla.org/mozilla-central/source/js/src/jit-test/tests/collections

// Flags: --harmony-arrays

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

  // %TypedArray%.of can be transplanted to other constructors.
  var hits = 0;
  function Bag(length) {
    assertEquals(arguments.length, 1);
    assertEquals(length, 2);
    this.length = length;
    hits++;
  }
  Bag.of = constructor.of;

  hits = 0;
  a = Bag.of("zero", "one");
  assertEquals(1, hits);
  assertEquals(2, a.length);
  assertArrayEquals(["zero", "one"], a);
  assertEquals(Bag.prototype, a.__proto__);

  hits = 0;
  actual = constructor.of.call(Bag, "zero", "one");
  assertEquals(1, hits);
  assertEquals(2, a.length);
  assertArrayEquals(["zero", "one"], a);
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
  function Pack() {
    Object.defineProperty(this, "length", {
      set: function (v) { status = "fail"; }
    });
  }
  Pack.of = constructor.of;
  var pack = Pack.of("wolves", "cards", "cigarettes", "lies");
  assertEquals("pass", status);

  // when the setter is on the new object's prototype
  function Bevy() {}
  Object.defineProperty(Bevy.prototype, "length", {
    set: function (v) { status = "fail"; }
  });
  Bevy.of = constructor.of;
  var bevy = Bevy.of("quail");
  assertEquals("pass", status);

  // Check superficial features of %TypedArray%.of.
  var desc = Object.getOwnPropertyDescriptor(constructor, "of");

  assertEquals(desc.configurable, false);
  assertEquals(desc.enumerable, false);
  assertEquals(desc.writable, false);
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
}

for (var constructor of typedArrayConstructors) {
  TestTypedArrayOf(constructor);
}
