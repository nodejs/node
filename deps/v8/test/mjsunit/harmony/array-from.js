// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-arrays --harmony-generators
(function() {

assertEquals(1, Array.from.length);

function assertArrayLikeEquals(value, expected, type) {
  assertInstanceof(value, type);
  assertEquals(expected.length, value.length);
  for (var i=0; i<value.length; ++i) {
    assertEquals(expected[i], value[i]);
  }
}

// Assert that constructor is called with "length" for array-like objects
var myCollectionCalled = false;
function MyCollection(length) {
  myCollectionCalled = true;
  assertEquals(1, arguments.length);
  assertEquals(5, length);
}

Array.from.call(MyCollection, {length: 5});
assertTrue(myCollectionCalled);

// Assert that calling mapfn with / without thisArg in sloppy and strict modes
// works as expected.
var global = this;
function non_strict(){ assertEquals(global, this); }
function strict(){ "use strict"; assertEquals(void 0, this); }
function strict_null(){ "use strict"; assertEquals(null, this); }
Array.from([1], non_strict);
Array.from([1], non_strict, void 0);
Array.from([1], non_strict, null);
Array.from([1], strict);
Array.from([1], strict, void 0);
Array.from([1], strict_null, null);

function testArrayFrom(thisArg, constructor) {
  assertArrayLikeEquals(Array.from.call(thisArg, [], undefined), [],
      constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, NaN), [], constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, Infinity), [], constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, 10000000), [], constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, 'test'), ['t', 'e', 's', 't'],
      constructor);

  assertArrayLikeEquals(Array.from.call(thisArg,
      { length: 1, '0': { 'foo': 'bar' } }), [{'foo': 'bar'}], constructor);

  assertArrayLikeEquals(Array.from.call(thisArg,
      { length: -1, '0': { 'foo': 'bar' } }), [], constructor);

  assertArrayLikeEquals(Array.from.call(thisArg,
      [ 'foo', 'bar', 'baz' ]), ['foo', 'bar', 'baz'], constructor);

  var kSet = new Set(['foo', 'bar', 'baz']);
  assertArrayLikeEquals(Array.from.call(thisArg, kSet), ['foo', 'bar', 'baz'],
      constructor);

  var kMap = new Map(['foo', 'bar', 'baz'].entries());
  assertArrayLikeEquals(Array.from.call(thisArg, kMap),
      [[0, 'foo'], [1, 'bar'], [2, 'baz']], constructor);


  function* generator() {
    yield 'a';
    yield 'b';
    yield 'c';
  }

  assertArrayLikeEquals(Array.from.call(thisArg, generator()),
                        ['a', 'b', 'c'], constructor);

  // Mozilla:
  // Array.from on a string handles surrogate pairs correctly.
  var gclef = "\uD834\uDD1E"; // U+1D11E MUSICAL SYMBOL G CLEF
  assertArrayLikeEquals(Array.from.call(thisArg, gclef), [gclef], constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, gclef + " G"),
      [gclef, " ", "G"], constructor);

  assertArrayLikeEquals(Array.from.call(thisArg, 'test', function(x) {
    return this.filter(x);
  }, {
    filter: function(x) { return x.toUpperCase(); }
  }), ['T', 'E', 'S', 'T'], constructor);
  assertArrayLikeEquals(Array.from.call(thisArg, 'test', function(x) {
    return x.toUpperCase();
  }), ['T', 'E', 'S', 'T'], constructor);

  this.thisArg = thisArg;
  assertThrows('Array.from.call(thisArg, null)', TypeError);
  assertThrows('Array.from.call(thisArg, undefined)', TypeError);
  assertThrows('Array.from.call(thisArg, [], null)', TypeError);
  assertThrows('Array.from.call(thisArg, [], "noncallable")', TypeError);

  this.nullIterator = {};
  nullIterator[Symbol.iterator] = null;
  assertThrows('Array.from.call(thisArg, nullIterator)', TypeError);

  this.nonObjIterator = {};
  nonObjIterator[Symbol.iterator] = function() { return "nonObject"; };
  assertThrows('Array.from.call(thisArg, nonObjIterator)', TypeError);

  assertThrows('Array.from.call(thisArg, [], null)', TypeError);
}

function Other() {}

var boundFn = (function() {}).bind(Array, 27);

testArrayFrom(Array, Array);
testArrayFrom(null, Array);
testArrayFrom({}, Array);
testArrayFrom(Object, Object);
testArrayFrom(Other, Other);
testArrayFrom(Math.cos, Array);
testArrayFrom(boundFn, Array);

})();
