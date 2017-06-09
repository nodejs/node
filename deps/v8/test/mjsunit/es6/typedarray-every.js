// Copyright 2014 the V8 project authors. All rights reserved.
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
  Float64Array];

function CheckTypedArrayIsNeutered(array) {
  assertEquals(0, array.byteLength);
  assertEquals(0, array.byteOffset);
  assertEquals(0, array.length);
}

function TestTypedArrayForEach(constructor) {
  assertEquals(1, constructor.prototype.every.length);

  var a = new constructor(3);
  a[0] = 0;
  a[1] = 1;
  a[2] = 2;

  var result = a.every(function (n) { return n < 2; });
  assertFalse(result);

  var result = a.every(function (n) { return n > 2; });
  assertFalse(result);

  var result = a.every(function (n) { return n >= 0; });
  assertEquals(true, result);

  // Use specified object as this object when calling the function.
  var o = { value: 42 };
  result = a.every(function (n, index, array) { return n == index && n < this.value; }, o);
  assertEquals(true, result);

  // Early exit happens when appropriate
  count = 0;
  result = a.every(function () { count++; return false; });
  assertEquals(1, count);
  assertFalse(result);

  // Modify the original array.
  count = 0;
  result = a.every(function (n, index, array) {
    array[index] = n + 1; count++; return true;
  });
  assertEquals(3, count);
  assertEquals(true, result);
  assertArrayEquals([1, 2, 3], a);

  // Check that values passed as second argument are wrapped into
  // objects when calling into sloppy mode functions.
  function CheckWrapping(value, wrapper) {
    var wrappedValue = new wrapper(value);

    a.every(function () {
      assertEquals("object", typeof this);
      assertEquals(wrappedValue, this);
    }, value);

    a.every(function () {
      "use strict";
      assertEquals(typeof value, typeof this);
      assertEquals(value, this);
    }, value);
  }
  CheckWrapping(true, Boolean);
  CheckWrapping(false, Boolean);
  CheckWrapping("xxx", String);
  CheckWrapping(42, Number);
  CheckWrapping(3.14, Number);
  CheckWrapping({}, Object);

  // Neutering the buffer backing the typed array mid-way should
  // still make .forEach() finish, and the array should keep being
  // empty after neutering it.
  count = 0;
  a = new constructor(3);
  result = a.every(function (n, index, array) {
    assertFalse(array[index] === undefined);  // don't get here if neutered
    if (count > 0) %ArrayBufferNeuter(array.buffer);
    array[index] = n + 1;
    count++;
    return count > 1 ? array[index] === undefined : true;
  });
  assertEquals(2, count);
  assertEquals(true, result);
  CheckTypedArrayIsNeutered(a);
  assertEquals(undefined, a[0]);

  // Calling array.buffer midway can change the backing store.
  a = new constructor(5);
  a[0] = 42;
  result = a.every(function (n, index, array) {
    assertSame(a, array);
    if (index == 2) {
      (new constructor(array.buffer))[(index + 1) % 5] = 42;
    } else {
      a[(index+1)%5] = 42
    }
    return n == 42;
  });
  assertEquals(true, result);

  // The method must work for typed arrays created from ArrayBuffer.
  // The length of the ArrayBuffer is chosen so it is a multiple of
  // all lengths of the typed array items.
  a = new constructor(new ArrayBuffer(64));
  count = 0;
  result = a.every(function (n) { return n == 0; });
  assertEquals(result, true);

  // Externalizing the array mid-way accessing the .buffer property
  // should work.
  a = new constructor(2);
  count = 0;
  var buffer = undefined;
  a.every(function (n, index, array) {
    if (count++ > 0)
      buffer = array.buffer;
    return true;
  });
  assertEquals(2, count);
  assertTrue(!!buffer);
  assertEquals("ArrayBuffer", %_ClassOf(buffer));
  assertSame(buffer, a.buffer);

  // The %TypedArray%.every() method should not work when
  // transplanted to objects that are not typed arrays.
  assertThrows(function () { constructor.prototype.every.call([1, 2, 3], function (x) {}) }, TypeError);
  assertThrows(function () { constructor.prototype.every.call("abc", function (x) {}) }, TypeError);
  assertThrows(function () { constructor.prototype.every.call({}, function (x) {}) }, TypeError);
  assertThrows(function () { constructor.prototype.every.call(0, function (x) {}) }, TypeError);

  // Method must be useable on instances of other typed arrays.
  for (var i = 0; i < typedArrayConstructors.length; i++) {
    count = 0;
    a = new typedArrayConstructors[i](4);
    constructor.prototype.every.call(a, function (x) { count++; return true; });
    assertEquals(a.length, count);
  }

  // Shadowing length doesn't affect every, unlike Array.prototype.every
  a = new constructor([1, 2]);
  Object.defineProperty(a, 'length', {value: 1});
  var x = 0;
  assertEquals(a.every(function(elt) { x += elt; return true; }), true);
  assertEquals(x, 3);
  assertEquals(Array.prototype.every.call(a,
      function(elt) { x += elt; return true; }), true);
  assertEquals(x, 4);

  // Detached Operation
  var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  %ArrayBufferNeuter(array.buffer);
  assertThrows(() => array.every(() => true), TypeError);
}

for (i = 0; i < typedArrayConstructors.length; i++) {
  TestTypedArrayForEach(typedArrayConstructors[i]);
}
