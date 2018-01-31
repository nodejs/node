// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-bigint --no-opt

'use strict'


// Without .toJSON method.

assertEquals(undefined, BigInt.prototype.toJSON);
assertThrows(() => JSON.stringify(42n), TypeError);
assertThrows(() => JSON.stringify(Object(42n)), TypeError);


// With .toJSON method that returns a string.

BigInt.prototype.toJSON = function() {
  assertEquals("bigint", typeof this);
  return String(this);
}
assertEquals("\"42\"", JSON.stringify(42n));

BigInt.prototype.toJSON = function() {
  assertEquals("object", typeof this);
  return String(this);
}
assertEquals("\"42\"", JSON.stringify(Object(42n)));


// With .toJSON method that returns a BigInt primitive.

BigInt.prototype.toJSON = function() {return this};
assertThrows(() => JSON.stringify(42n), TypeError);
assertThrows(() => JSON.stringify(Object(42n)), TypeError);


// With .toJSON method that returns a BigInt object.

BigInt.prototype.toJSON = function() {return Object(this)};
assertThrows(() => JSON.stringify(42n), TypeError);
assertThrows(() => JSON.stringify(Object(42n)), TypeError);


// Without .toJSON method but with a replacer returning a string.

delete BigInt.prototype.toJSON;
let replacer;

replacer = function(k, v) {
  assertEquals("bigint", typeof v);
  assertTrue(42n == v);
  return "43";
}
assertEquals("\"43\"", JSON.stringify(42n, replacer));

replacer = function(k, v) {
  assertEquals("object", typeof v);
  assertTrue(42n == v);
  return "43";
}
assertEquals("\"43\"", JSON.stringify(Object(42n), replacer));


// Without .toJSON method but with a replacer returning a BigInt primitive.

assertEquals(undefined, BigInt.prototype.toJSON);

replacer = () => 43n;
assertThrows(() => JSON.stringify(42n, replacer), TypeError);
assertThrows(() => JSON.stringify(Object(42n), replacer), TypeError);


// Without .toJSON method but with a replacer returning a BigInt object.

assertEquals(undefined, BigInt.prototype.toJSON);

replacer = () => Object(43n);
assertThrows(() => JSON.stringify(42n, replacer), TypeError);
assertThrows(() => JSON.stringify(Object(42n), replacer), TypeError);
