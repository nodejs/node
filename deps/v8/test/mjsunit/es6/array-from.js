// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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

  assertThrows(function() { Array.from.call(thisArg, null); }, TypeError);
  assertThrows(function() { Array.from.call(thisArg, undefined); }, TypeError);
  assertThrows(function() { Array.from.call(thisArg, [], null); }, TypeError);
  assertThrows(function() { Array.from.call(thisArg, [], "noncallable"); },
               TypeError);

  var nullIterator = {};
  nullIterator[Symbol.iterator] = null;
  assertArrayLikeEquals(Array.from.call(thisArg, nullIterator), [],
                        constructor);

  var nonObjIterator = {};
  nonObjIterator[Symbol.iterator] = function() { return "nonObject"; };
  assertThrows(function() { Array.from.call(thisArg, nonObjIterator); },
               TypeError);

  assertThrows(function() { Array.from.call(thisArg, [], null); }, TypeError);

  // Ensure iterator is only accessed once, and only invoked once
  var called = false;
  var arr = [1, 2, 3];
  var obj = {};

  // Test order --- only get iterator method once
  function testIterator() {
    assertFalse(called, "@@iterator should be called only once");
    called = true;
    assertEquals(obj, this);
    return arr[Symbol.iterator]();
  }
  var getCalled = false;
  Object.defineProperty(obj, Symbol.iterator, {
    get: function() {
      assertFalse(getCalled, "@@iterator should be accessed only once");
      getCalled = true;
      return testIterator;
    },
    set: function() {
      assertUnreachable("@@iterator should not be set");
    }
  });
  assertArrayLikeEquals(Array.from.call(thisArg, obj), [1, 2, 3], constructor);
}

function Other() {}

var boundFn = (function() {}).bind(Array, 27);

testArrayFrom(Array, Array);
testArrayFrom(null, Array);
testArrayFrom({}, Array);
testArrayFrom(Object, Object);
testArrayFrom(Other, Other);
testArrayFrom(Math.cos, Array);
testArrayFrom(Math.cos.bind(Math), Array);
testArrayFrom(boundFn, boundFn);

// Assert that [[DefineOwnProperty]] is used in ArrayFrom, meaning a
// setter isn't called, and a failed [[DefineOwnProperty]] will throw.
var setterCalled = 0;
function exotic() {
  Object.defineProperty(this,  '0', {
    get: function() { return 2; },
    set: function() { setterCalled++; }
  });
}
// Non-configurable properties can't be overwritten with DefineOwnProperty
assertThrows(function () { Array.from.call(exotic, [1]); }, TypeError);
// The setter wasn't called
assertEquals(0, setterCalled);

// Non-callable iterators should cause a TypeError before calling the target
// constructor.
items = {};
items[Symbol.iterator] = 7;
function TestError() {}
function ArrayLike() { throw new TestError() }
assertThrows(function() { Array.from.call(ArrayLike, items); }, TypeError);

// Check that array properties defined are writable, enumerable, configurable
function ordinary() { }
var x = Array.from.call(ordinary, [2]);
var xlength = Object.getOwnPropertyDescriptor(x, 'length');
assertEquals(1, xlength.value);
assertEquals(true, xlength.writable);
assertEquals(true, xlength.enumerable);
assertEquals(true, xlength.configurable);
var x0 = Object.getOwnPropertyDescriptor(x, 0);
assertEquals(2, x0.value);
assertEquals(true, xlength.writable);
assertEquals(true, xlength.enumerable);
assertEquals(true, xlength.configurable);

})();

(function testElementsKind() {
  // Check that Array.from returns PACKED elements.
  var arr = Array.from([1,2,3]);
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasSmiElements(arr));

  var arr = Array.from({length: 3});
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasObjectElements(arr));

  var arr = Array.from({length: 3}, (x) => 1);
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasSmiElements(arr));

  var arr = Array.from({length: 3}, (x) => 1.5);
  assertTrue(%HasFastPackedElements(arr));
  assertTrue(%HasDoubleElements(arr));
})();
