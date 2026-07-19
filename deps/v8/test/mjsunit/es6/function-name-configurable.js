// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getStrictF() {
  'use strict';
  return function f() {};
}


function getSloppyF() {
  return function f() {};
}


function test(testFunction) {
  testFunction(getStrictF());
  testFunction(getSloppyF());
}


function testDescriptor(f) {
  var descr = Object.getOwnPropertyDescriptor(f, 'name');
  assertTrue(descr.configurable);
  assertFalse(descr.enumerable);
  assertEquals('f', descr.value);
  assertFalse(descr.writable);
}
test(testDescriptor);


function testSet(f) {
  f.name = 'g';
  assertEquals('f', f.name);
}
test(testSet);


function testSetStrict(f) {
  'use strict';
  assertThrows(function() {
    f.name = 'g';
  }, TypeError);
}
test(testSetStrict);


function testReconfigureAsDataProperty(f) {
  Object.defineProperty(f, 'name', {
    value: 'g',
  });
  assertEquals('g', f.name);
  Object.defineProperty(f, 'name', {
    writable: true
  });
  f.name = 'h';
  assertEquals('h', f.name);

  f.name = 42;
  assertEquals(42, f.name);
}
test(testReconfigureAsDataProperty);


function testReconfigureAsAccessorProperty(f) {
  var name = 'g';
  Object.defineProperty(f, 'name', {
    get: function() { return name; },
    set: function(v) { name = v; }
  });
  assertEquals('g', f.name);
  f.name = 'h';
  assertEquals('h', f.name);
}
test(testReconfigureAsAccessorProperty);


function testFunctionToString(f) {
  Object.defineProperty(f, 'name', {
    value: {toString: function() { assertUnreachable(); }},
  });
  assertEquals('function f() {}', f.toString());
}
test(testFunctionToString);


(function testSetOnInstance() {
  // This needs to come before testDelete below
  assertTrue(Function.prototype.hasOwnProperty('name'));

  function f() {}
  delete f.name;
  assertEquals('', f.name);

  f.name = 42;
  assertEquals('', f.name);  // non writable prototype property.
  assertFalse(f.hasOwnProperty('name'));

  Object.defineProperty(Function.prototype, 'name', {writable: true});

  f.name = 123;
  assertTrue(f.hasOwnProperty('name'));
  assertEquals(123, f.name);
})();


(function testDelete() {
  function f() {}
  assertTrue(delete f.name);
  assertFalse(f.hasOwnProperty('name'));
  assertEquals('', f.name);

  assertTrue(delete Function.prototype.name);
  assertEquals(undefined, f.name);
})();
