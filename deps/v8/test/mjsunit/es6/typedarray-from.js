// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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

function defaultValue(constr) {
  if (constr == Float32Array || constr == Float64Array) return NaN;
  return 0;
}

function assertArrayLikeEquals(value, expected, type) {
  assertEquals(value.__proto__, type.prototype);
  assertEquals(expected.length, value.length);
  for (var i = 0; i < value.length; ++i) {
    assertEquals(expected[i], value[i]);
  }
}

(function() {
  var source = [-0, 0, 2**40, 2**41, 2**42];
  var arr = Float64Array.from(source);
  assertArrayLikeEquals(arr, source, Float64Array);

  arr = Float32Array.from(source);
  assertArrayLikeEquals(arr, source, Float32Array);
})();

(function() {
  var source = [-0, 0, , 2**42];
  var expected = [-0, 0, NaN, 2**42];
  var arr = Float64Array.from(source);
  assertArrayLikeEquals(arr, expected, Float64Array);

  arr = Float32Array.from(source);
  assertArrayLikeEquals(arr, expected, Float32Array);
})();

(function() {
  var source = {0: -0, 1: 0, 2: 2**40, 3: 2**41, 4: 2**42, length: 5};
  var expected = [-0, 0, 2**40, 2**41, 2**42];
  var arr = Float64Array.from(source);
  assertArrayLikeEquals(arr, expected, Float64Array);

  arr = Float32Array.from(source);
  assertArrayLikeEquals(arr, expected, Float32Array);
})();

(function() {
  var source = [-0, 0, , 2**42];
  Object.getPrototypeOf(source)[2] = 27;
  var expected = [-0, 0, 27, 2**42];
  var arr = Float64Array.from(source);
  assertArrayLikeEquals(arr, expected, Float64Array);

  arr = Float32Array.from(source);
  assertArrayLikeEquals(arr, expected, Float32Array);
})();

for (var constructor of typedArrayConstructors) {
  assertEquals(1, constructor.from.length);

  // TypedArray.from only callable on this subclassing %TypedArray%
  assertThrows(function () {constructor.from.call(Array, [])}, TypeError);


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

  // Use a map function that returns non-numbers.
  function mapper(value, index) {
    return String.fromCharCode(value);
  }
  var d = defaultValue(constructor);
  assertArrayLikeEquals(
      constructor.from([72, 69, 89], mapper), [d, d, d], constructor);

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

  // Check mapper is used for non-iterator case.
  function mapper2(value, index) {
    return value + 1;
  }
  var array_like = {
    0: 1,
    1: 2,
    2: 3,
    length: 3
  };
  assertArrayLikeEquals(constructor.from(array_like, mapper2),
                        [2, 3, 4], constructor);

  // With a smi source. Step 10 will set len = 0.
  var source = 1;
  assertArrayLikeEquals(constructor.from(source), [], constructor);

  assertThrows(function() { constructor.from(null); }, TypeError);
  assertThrows(function() { constructor.from(undefined); }, TypeError);
  assertThrows(function() { constructor.from([], null); }, TypeError);
  assertThrows(function() { constructor.from([], 'noncallable'); },
               TypeError);

  source = [1, 2, 3];
  var proxy = new Proxy(source, {});
  assertArrayLikeEquals(constructor.from(proxy), source, constructor);

  proxy = new Proxy(source, {
    get: function(target, name) {
      if (name === Symbol.iterator) return target[name];
      if (name === "length") return 3;
      return target[name] + 1;
    }
  });
  assertArrayLikeEquals(constructor.from(proxy), [2, 3, 4], constructor);
}

// Tests that modify global state in a way that affects fast paths e.g. by
// invalidating protectors or changing prototypes.
for (var constructor of typedArrayConstructors) {
  source = [1, 2, 3];
  source[Symbol.iterator] = undefined;
  assertArrayLikeEquals(constructor.from(source), source, constructor);

  source = [{ valueOf: function(){ return 42; }}];
  source[Symbol.iterator] = undefined;
  assertArrayLikeEquals(constructor.from(source), [42], constructor);

  Number.prototype[Symbol.iterator] = function* () {
    yield 1;
    yield 2;
    yield 3;
  }
  assertArrayLikeEquals(constructor.from(1), [1, 2, 3], constructor);
  assertArrayLikeEquals(constructor.from(1.1), [1, 2, 3], constructor);

  var nullIterator = {};
  nullIterator[Symbol.iterator] = null;
  assertArrayLikeEquals(constructor.from(nullIterator), [],
                        constructor);

  var nonObjIterator = {};
  nonObjIterator[Symbol.iterator] = function() { return 'nonObject'; };
  assertThrows(function() { constructor.from(nonObjIterator); },
               TypeError);

  assertThrows(function() { constructor.from([], null); }, TypeError);

  d = defaultValue(constructor);
  let ta1 = new constructor(3).fill(1);
  Object.defineProperty(ta1, "length", {get: function() {
    return 6;
  }});
  delete ta1[Symbol.iterator];
  delete ta1.__proto__[Symbol.iterator];
  delete ta1.__proto__.__proto__[Symbol.iterator];
  assertArrayLikeEquals(constructor.from(ta1), [1, 1, 1, d, d, d], constructor);

  let ta2 = new constructor(3).fill(1);
  Object.defineProperty(ta2, "length", {get: function() {
    %ArrayBufferNeuter(ta2.buffer);
    return 6;
  }});
  assertArrayLikeEquals(constructor.from(ta2), [d, d, d, d, d, d], constructor);

  var o1 = {0: 0, 1: 1, 2: 2, length: 6};
  assertArrayLikeEquals(constructor.from(o1), [0, 1, 2, d, d, d], constructor);

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
