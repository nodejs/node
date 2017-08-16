// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getStrictF() {
  'use strict';
  return function f(x) {};
}


function getSloppyF() {
  return function f(x) {};
}


function getStrictGenerator() {
  'use strict';
  return function* f(x) {};
}


function getSloppyGenerator() {
  return function* f(x) {};
}


function test(testFunction) {
  testFunction(getStrictF());
  testFunction(getSloppyF());
  testFunction(getStrictGenerator());
  testFunction(getSloppyGenerator());
}


function testDescriptor(f) {
  var descr = Object.getOwnPropertyDescriptor(f, 'length');
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals(1, descr.value);
  assertFalse(descr.writable);
}
test(testDescriptor);


function testSet(f) {
  f.length = 2;
  assertEquals(1, f.length);
}
test(testSet);


function testSetStrict(f) {
  'use strict';
  assertThrows(function() {
    f.length = 2;
  }, TypeError);
}
test(testSetStrict);


function testReconfigureAsDataProperty(f) {
  Object.defineProperty(f, 'length', {
    value: 2,
  });
  assertEquals(2, f.length);
  Object.defineProperty(f, 'length', {
    writable: true
  });
  f.length = 3;
  assertEquals(3, f.length);

  f.length = 42;
  assertEquals(42, f.length);
}
test(testReconfigureAsDataProperty);


function testReconfigureAsAccessorProperty(f) {
  var length = 2;
  Object.defineProperty(f, 'length', {
    get: function() { return length; },
    set: function(v) { length = v; }
  });
  assertEquals(2, f.length);
  f.length = 3;
  assertEquals(3, f.length);
}
test(testReconfigureAsAccessorProperty);


(function testSetOnInstance() {
  // This needs to come before testDelete below
  assertTrue(Function.prototype.hasOwnProperty('length'));

  function f() {}
  delete f.length;
  assertEquals(0, f.length);

  f.length = 42;
  assertEquals(0, f.length);  // non writable prototype property.
  assertFalse(f.hasOwnProperty('length'));

  Object.defineProperty(Function.prototype, 'length', {writable: true});

  f.length = 123;
  assertTrue(f.hasOwnProperty('length'));
  assertEquals(123, f.length);
})();


(function testDelete() {
  function f(x) {}
  assertTrue(delete f.length);
  assertFalse(f.hasOwnProperty('length'));
  assertEquals(0, f.length);

  assertTrue(delete Function.prototype.length);
  assertEquals(undefined, f.length);
})();
