// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var typedArrayConstructors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Uint8ClampedArray,
  Float32Array,
  Float64Array
];

for (var constructor of typedArrayConstructors) {
  assertEquals(1, constructor.from.length);

  // TypedArray.from only callable on this subclassing %TypedArray%
  assertThrows(function () {constructor.from.call(Array, [])}, TypeError);

  function assertArrayLikeEquals(value, expected, type) {
    assertEquals(value.__proto__, type.prototype);
    assertEquals(expected.length, value.length);
    for (var i = 0; i < value.length; ++i) {
      assertEquals(expected[i], value[i]);
    }
  }

  // Assert that calling mapfn with / without thisArg in sloppy and strict modes
  // works as expected.
  var global = this;
  function non_strict() { assertEquals(global, this); }
  function strict() { 'use strict'; assertEquals(undefined, this); }
  function strict_null() { 'use strict'; assertEquals(null, this); }
  constructor.from([1], non_strict);
  constructor.from([1], non_strict, void 0);
  constructor.from([1], non_strict, null);
  constructor.from([1], strict);
  constructor.from([1], strict, void 0);
  constructor.from([1], strict_null, null);

  // TypedArray.from can only be called on TypedArray constructors
  assertThrows(function() {constructor.from.call({}, [])}, TypeError);
  assertThrows(function() {constructor.from.call([], [])}, TypeError);
  assertThrows(function() {constructor.from.call(1, [])}, TypeError);
  assertThrows(function() {constructor.from.call(undefined, [])}, TypeError);

  // Converting from various other types, demonstrating that it can
  // operate on array-like objects as well as iterables.
  // TODO(littledan): constructors should have similar flexibility.
  assertArrayLikeEquals(constructor.from(
      { length: 1, 0: 5 }), [5], constructor);

  assertArrayLikeEquals(constructor.from(
      { length: -1, 0: 5 }), [], constructor);

  assertArrayLikeEquals(constructor.from(
      [1, 2, 3]), [1, 2, 3], constructor);

  var set = new Set([1, 2, 3]);
  assertArrayLikeEquals(constructor.from(set), [1, 2, 3],
        constructor);

  function* generator() {
    yield 4;
    yield 5;
    yield 6;
  }

  assertArrayLikeEquals(constructor.from(generator()),
                        [4, 5, 6], constructor);

  assertThrows(function() { constructor.from(null); }, TypeError);
  assertThrows(function() { constructor.from(undefined); }, TypeError);
  assertThrows(function() { constructor.from([], null); }, TypeError);
  assertThrows(function() { constructor.from([], 'noncallable'); },
               TypeError);

  var nullIterator = {};
  nullIterator[Symbol.iterator] = null;
  assertArrayLikeEquals(constructor.from(nullIterator), [],
                        constructor);

  var nonObjIterator = {};
  nonObjIterator[Symbol.iterator] = function() { return 'nonObject'; };
  assertThrows(function() { constructor.from(nonObjIterator); },
               TypeError);

  assertThrows(function() { constructor.from([], null); }, TypeError);

  // Ensure iterator is only accessed once, and only invoked once
  var called = 0;
  var arr = [1, 2, 3];
  var obj = {};
  var counter = 0;

  // Test order --- only get iterator method once
  function testIterator() {
    called++;
    assertEquals(obj, this);
    return arr[Symbol.iterator]();
  }
  var getCalled = 0;
  Object.defineProperty(obj, Symbol.iterator, {
    get: function() {
      getCalled++;
      return testIterator;
    },
    set: function() {
      assertUnreachable('@@iterator should not be set');
    }
  });
  assertArrayLikeEquals(constructor.from(obj), [1, 2, 3], constructor);
  assertEquals(getCalled, 1);
  assertEquals(called, 1);

  assertEquals(constructor, Uint8Array.from.call(constructor, [1]).constructor);
  assertEquals(Uint8Array, constructor.from.call(Uint8Array, [1]).constructor);
}
