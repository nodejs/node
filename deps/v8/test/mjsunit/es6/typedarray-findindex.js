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

for (var constructor of typedArrayConstructors) {

assertEquals(1, constructor.prototype.findIndex.length);

var a = new constructor([21, 22, 23, 24]);
assertEquals(-1, a.findIndex(function() { return false; }));
assertEquals(-1, a.findIndex(function(val) { return 121 === val; }));
assertEquals(0, a.findIndex(function() { return true; }));
assertEquals(1, a.findIndex(function(val) { return 22 === val; }), undefined);
assertEquals(2, a.findIndex(function(val) { return 23 === val; }), null);
assertEquals(3, a.findIndex(function(val) { return 24 === val; }));


//
// Test predicate is not called when array is empty
//
(function() {
  var a = new constructor([]);
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;

  a.findIndex(function(val, key, obj) {
    o = obj;
    l = obj.length;
    v = val;
    k = key;

    return false;
  });

  assertEquals(-1, l);
  assertEquals(-1, o);
  assertEquals(-1, v);
  assertEquals(-1, k);
})();


//
// Test predicate is called with correct argumetns
//
(function() {
  var a = new constructor([5]);
  var l = -1;
  var o = -1;
  var v = -1;
  var k = -1;

  var index = a.findIndex(function(val, key, obj) {
    o = obj;
    l = obj.length;
    v = val;
    k = key;

    return false;
  });

  assertArrayEquals(a, o);
  assertEquals(a.length, l);
  assertEquals(5, v);
  assertEquals(0, k);
  assertEquals(-1, index);
})();


//
// Test predicate is called array.length times
//
(function() {
  var a = new constructor([1, 2, 3, 4, 5]);
  var l = 0;

  a.findIndex(function() {
    l++;
    return false;
  });

  assertEquals(a.length, l);
})();


//
// Test array modifications
//
(function() {
  a = new constructor([1, 2, 3]);
  a.findIndex(function(val, key) { a[key] = ++val; return false; });
  assertArrayEquals([2, 3, 4], a);
  assertEquals(3, a.length);
})();


//
// Test thisArg
//
(function() {
  // Test String as a thisArg
  var index = new constructor([1, 2, 3]).findIndex(function(val, key) {
    return this.charAt(Number(key)) === String(val);
  }, "321");
  assertEquals(1, index);

  // Test object as a thisArg
  var thisArg = {
    elementAt: function(key) {
      return this[key];
    }
  };
  Array.prototype.push.apply(thisArg, [3, 2, 1]);

  index = new constructor([1, 2, 3]).findIndex(function(val, key) {
    return this.elementAt(key) === val;
  }, thisArg);
  assertEquals(1, index);

  // Create a new object in each function call when receiver is a
  // primitive value. See ECMA-262, Annex C.
  a = [];
  new constructor([1, 2]).findIndex(function() { a.push(this) }, "");
  assertTrue(a[0] !== a[1]);

  // Do not create a new object otherwise.
  a = [];
  new constructor([1, 2]).findIndex(function() { a.push(this) }, {});
  assertEquals(a[0], a[1]);

  // In strict mode primitive values should not be coerced to an object.
  a = [];
  new constructor([1, 2]).findIndex(function() { 'use strict'; a.push(this); }, "");
  assertEquals("", a[0]);
  assertEquals(a[0], a[1]);

})();

// Test exceptions
assertThrows('constructor.prototype.findIndex.call(null, function() { })',
  TypeError);
assertThrows('constructor.prototype.findIndex.call(undefined, function() { })',
  TypeError);
assertThrows('constructor.prototype.findIndex.apply(null, function() { }, [])',
  TypeError);
assertThrows('constructor.prototype.findIndex.apply(undefined, function() { }, [])',
  TypeError);
assertThrows('constructor.prototype.findIndex.apply([], function() { }, [])',
  TypeError);
assertThrows('constructor.prototype.findIndex.apply({}, function() { }, [])',
  TypeError);
assertThrows('constructor.prototype.findIndex.apply("", function() { }, [])',
  TypeError);

assertThrows('new constructor([]).findIndex(null)', TypeError);
assertThrows('new constructor([]).findIndex(undefined)', TypeError);
assertThrows('new constructor([]).findIndex(0)', TypeError);
assertThrows('new constructor([]).findIndex(true)', TypeError);
assertThrows('new constructor([]).findIndex(false)', TypeError);
assertThrows('new constructor([]).findIndex("")', TypeError);
assertThrows('new constructor([]).findIndex({})', TypeError);
assertThrows('new constructor([]).findIndex([])', TypeError);
assertThrows('new constructor([]).findIndex(/\d+/)', TypeError);

// Shadowing length doesn't affect findIndex, unlike Array.prototype.findIndex
a = new constructor([1, 2]);
Object.defineProperty(a, 'length', {value: 1});
var x = 0;
assertEquals(a.findIndex(function(elt) { x += elt; return false; }), -1);
assertEquals(x, 3);
assertEquals(Array.prototype.findIndex.call(a,
    function(elt) { x += elt; return false; }), -1);
assertEquals(x, 4);

// Detached Operation
  var tmp = {
    [Symbol.toPrimitive]() {
      assertUnreachable("Parameter should not be processed when " +
                        "array.[[ViewedArrayBuffer]] is detached.");
      return 0;
    }
  };
  var array = new constructor([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  %ArrayBufferDetach(array.buffer);
  assertThrows(() => array.findIndex(tmp), TypeError);
}
