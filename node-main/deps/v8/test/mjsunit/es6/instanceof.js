// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Make sure it's an error if @@hasInstance isn't a function.
(function() {
  var F = {};
  F[Symbol.hasInstance] = null;
  assertThrows(function() { 0 instanceof F; }, TypeError);
})();

// Make sure the result is coerced to boolean.
(function() {
  var F = {};
  F[Symbol.hasInstance] = function() { return undefined; };
  assertEquals(0 instanceof F, false);
  F[Symbol.hasInstance] = function() { return null; };
  assertEquals(0 instanceof F, false);
  F[Symbol.hasInstance] = function() { return true; };
  assertEquals(0 instanceof F, true);
})();

// Make sure if @@hasInstance throws, we catch it.
(function() {
  var F = {};
  F[Symbol.hasInstance] = function() { throw new Error("always throws"); }
  try {
    0 instanceof F;
  } catch (e) {
    assertEquals(e.message, "always throws");
  }
})();

// @@hasInstance works for bound functions.
(function() {
  var BC = function() {};
  var bc = new BC();
  var bound = BC.bind();
  assertEquals(bound[Symbol.hasInstance](bc), true);
  assertEquals(bound[Symbol.hasInstance]([]), false);
})();

// if OrdinaryHasInstance is passed a non-callable receiver, return false.
assertEquals(Function.prototype[Symbol.hasInstance].call(Array, []), true);
assertEquals(Function.prototype[Symbol.hasInstance].call({}, {}), false);

// OrdinaryHasInstance passed a non-object argument returns false.
assertEquals(Function.prototype[Symbol.hasInstance].call(Array, 0), false);

// Cannot assign to @@hasInstance with %FunctionPrototype%.
(function() {
  "use strict";
  function F() {}
  assertThrows(function() { F[Symbol.hasInstance] = (v) => v }, TypeError);
})();

// Check correct invocation of @@hasInstance handler on function instance.
(function() {
  function F() {}
  var counter = 0;
  var proto = Object.getPrototypeOf(F);
  Object.setPrototypeOf(F, null);
  F[Symbol.hasInstance] = function(v) { ++counter; return true };
  Object.setPrototypeOf(F, proto);
  assertTrue(1 instanceof F);
  assertEquals(1, counter);
})();
