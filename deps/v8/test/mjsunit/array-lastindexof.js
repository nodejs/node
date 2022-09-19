// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows(() => {
  Array.prototype.lastIndexOf.call(null, 42);
}, TypeError);
assertThrows(() => {
  Array.prototype.lastIndexOf.call(undefined, 42);
}, TypeError);

/* Tests inspired by test262's
  lastIndexOf/calls-only-has-on-prototype-after-length-zeroed.js */
// Stateful fromIndex that tries to empty the array
(function testFromIndex() {
  var array = [5, undefined, 7];
  var fromIndex = {
    valueOf: function() {
      array.length = 1;
      return 2;
    }
  };
  assertEquals(-1, array.lastIndexOf(undefined, fromIndex));

  array = [5, undefined, 7];
  assertEquals(0, array.lastIndexOf(5, fromIndex));
})();

// Stateful fromIndex and proxy as Prototype
// Must test for [[HasProperty]] before [[Get]]
var testHasProperty = function(value) {
  var array = [5, undefined, 7];
  var fromIndex = {
    valueOf: function() {
      array.length = 0;
      return 2;
    }
  };

  // Install a prototype that only has [[HasProperty]], and throws on [[Get]]
  Object.setPrototypeOf(array,
    new Proxy(Array.prototype, {
                has: function(t, pk) { return pk in t; },
                get: function () { throw new Error('[[Get]] trap called') },
              }));

  assertEquals(-1, Array.prototype.lastIndexOf.call(array, value, fromIndex));
}

testHasProperty(5);
testHasProperty(undefined);

// Test call order: [[HasProperty]] before [[Get]]
var testHasPropertyThenGet = function(value) {
  var array = [5, , 7];
  var log = [];

  // Install a prototype with only [[HasProperty]] and [[Get]]
  Object.setPrototypeOf(array,
    new Proxy(Array.prototype, {
                has: function() { log.push("HasProperty"); return true; },
                get: function() { log.push("Get"); },
              }));
  // The 2nd element (index 1) will trigger the calls to the prototype
  Array.prototype.lastIndexOf.call(array, value);
  assertEquals(["HasProperty", "Get"], log);
}

testHasPropertyThenGet(5);
testHasPropertyThenGet(undefined);

// Test for sparse Arrays
/* This will not enter the fast path for sparse arrays, due to UseSparseVariant
  excluding array elements with accessors */
(function() {
  var array = new Array(10000);
  array[0] = 5; array[9999] = 7;

  var count = 0;
  Object.defineProperty(array.__proto__, 9998, { get: () => ++count });
  Array.prototype.lastIndexOf.call(array, 0);
  assertEquals(1,count);
})();
