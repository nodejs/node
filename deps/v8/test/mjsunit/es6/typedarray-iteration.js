// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for standard TypedArray array iteration functions.

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

function assertArrayLikeEquals(expected, value, type) {
  assertEquals(value.__proto__, type.prototype);
  assertEquals(expected.length, value.length);
  for (var i = 0; i < value.length; ++i) {
    assertEquals(expected[i], value[i]);
  }
}

for (var constructor of typedArrayConstructors) {
  (function TypedArrayFilterTest() {
    // Simple use.
    var a = new constructor([0, 1]);
    assertArrayLikeEquals([0], a.filter(function(n) { return n == 0; }),
                          constructor);
    assertArrayLikeEquals([0, 1], a, constructor);

    // Use specified object as this object when calling the function.
    var o = { value: 42 }
    a = new constructor([1, 42, 3, 42, 4]);
    assertArrayLikeEquals([42, 42], a.filter(function(n) {
      return this.value == n
    }, o), constructor);

    // Modify original array.
    a = new constructor([1, 42, 3, 42, 4]);
    assertArrayLikeEquals([42, 42], a.filter(function(n, index, array) {
      array[index] = 43; return 42 == n;
    }), constructor);
    assertArrayLikeEquals([43, 43, 43, 43, 43], a, constructor);

    // Create a new object in each function call when receiver is a
    // primitive value. See ECMA-262, Annex C.
    a = [];
    new constructor([1, 2]).filter(function() { a.push(this) }, '');
    assertTrue(a[0] !== a[1]);

    // Do not create a new object otherwise.
    a = [];
    new constructor([1, 2]).filter(function() { a.push(this) }, {});
    assertEquals(a[0], a[1]);

    // In strict mode primitive values should not be coerced to an object.
    a = [];
    new constructor([1, 2]).filter(function() {
      'use strict';
      a.push(this);
    }, '');
    assertEquals('', a[0]);
    assertEquals(a[0], a[1]);

    // Calling this method on other types is a TypeError
    assertThrows(function() {
      constructor.prototype.filter.call([], function() {});
    }, TypeError);

    // Shadowing the length property doesn't change anything
    a = new constructor([1, 2]);
    Object.defineProperty(a, 'length', { value: 1 });
    assertArrayLikeEquals([2], a.filter(function(elt) {
      return elt == 2;
    }), constructor);

    // Detached Operation
    var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    %ArrayBufferDetach(array.buffer);
    assertThrows(() => array.filter(() => false), TypeError);
  })();

  (function TypedArrayMapTest() {
    var a = new constructor([0, 1, 2, 3, 4]);

    // Simple use.
    var result = [1, 2, 3, 4, 5];
    assertArrayLikeEquals(result, a.map(function(n) { return n + 1; }),
                          constructor);
    assertEquals(a, a);

    // Use specified object as this object when calling the function.
    var o = { delta: 42 }
    result = [42, 43, 44, 45, 46];
    assertArrayLikeEquals(result, a.map(function(n) {
      return this.delta + n;
    }, o), constructor);

    // Modify original array.
    a = new constructor([0, 1, 2, 3, 4]);
    result = [1, 2, 3, 4, 5];
    assertArrayLikeEquals(result, a.map(function(n, index, array) {
      array[index] = n + 1;
      return n + 1;
    }), constructor);
    assertArrayLikeEquals(result, a, constructor);

    // Create a new object in each function call when receiver is a
    // primitive value. See ECMA-262, Annex C.
    a = [];
    new constructor([1, 2]).map(function() { a.push(this) }, '');
    assertTrue(a[0] !== a[1]);

    // Do not create a new object otherwise.
    a = [];
    new constructor([1, 2]).map(function() { a.push(this) }, {});
    assertEquals(a[0], a[1]);

    // In strict mode primitive values should not be coerced to an object.
    a = [];
    new constructor([1, 2]).map(function() { 'use strict'; a.push(this); }, '');
    assertEquals('', a[0]);
    assertEquals(a[0], a[1]);

    // Test that the result is converted to the right type
    assertArrayLikeEquals([3, 3], new constructor([1, 2]).map(function() {
      return "3";
    }), constructor);
    if (constructor !== Float32Array && constructor !== Float64Array) {
      assertArrayLikeEquals([0, 0], new constructor([1, 2]).map(function() {
        return NaN;
      }), constructor);
    }

    // Detached Operation
    var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    %ArrayBufferDetach(array.buffer);
    assertThrows(() => array.map((v) => v), TypeError);
  })();

  //
  // %TypedArray%.prototype.some
  //
  (function TypedArraySomeTest() {
    var a = new constructor([0, 1, 2, 3, 4]);

    // Simple use.
    assertTrue(a.some(function(n) { return n == 3}));
    assertFalse(a.some(function(n) { return n == 5}));

    // Use specified object as this object when calling the function.
    var o = { element: 42 };
    a = new constructor([1, 42, 3]);
    assertTrue(a.some(function(n) { return this.element == n; }, o));
    a = new constructor([1]);
    assertFalse(a.some(function(n) { return this.element == n; }, o));

    // Modify original array.
    a = new constructor([0, 1, 2, 3]);
    assertTrue(a.some(function(n, index, array) {
      array[index] = n + 1;
      return n == 2;
    }));
    assertArrayLikeEquals([1, 2, 3, 3], a, constructor);

    // Create a new object in each function call when receiver is a
    // primitive value. See ECMA-262, Annex C.
    a = [];
    new constructor([1, 2]).some(function() { a.push(this) }, '');
    assertTrue(a[0] !== a[1]);

    // Do not create a new object otherwise.
    a = [];
    new constructor([1, 2]).some(function() { a.push(this) }, {});
    assertEquals(a[0], a[1]);

    // In strict mode primitive values should not be coerced to an object.
    a = [];
    new constructor([1, 2]).some(function() {
      'use strict';
      a.push(this);
    }, '');
    assertEquals('', a[0]);
    assertEquals(a[0], a[1]);

    // Calling this method on other types is a TypeError
    assertThrows(function() {
      constructor.prototype.some.call([], function() {});
    }, TypeError);

    // Shadowing the length property doesn't change anything
    a = new constructor([1, 2]);
    Object.defineProperty(a, 'length', { value: 1 });
    assertEquals(true, a.some(function(elt) { return elt == 2; }));
    assertEquals(false, Array.prototype.some.call(a, function(elt) {
      return elt == 2;
    }));

    // Detached Operation
    var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    %ArrayBufferDetach(array.buffer);
    assertThrows(() => array.some((v) => false), TypeError);
  })();

}
