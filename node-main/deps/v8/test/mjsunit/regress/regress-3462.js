// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function TestFunctionPrototypeSetter() {
  var f = function() {};
  var o = {__proto__: f};
  o.prototype = 42;
  assertEquals(42, o.prototype);
  assertTrue(o.hasOwnProperty('prototype'));
}
TestFunctionPrototypeSetter();


function TestFunctionPrototypeSetterOnValue() {
  var f = function() {};
  var fp = f.prototype;
  Number.prototype.__proto__ = f;
  var n = 42;
  var o = {};
  n.prototype = o;
  assertEquals(fp, n.prototype);
  assertEquals(fp, f.prototype);
  assertFalse(Number.prototype.hasOwnProperty('prototype'));
}
TestFunctionPrototypeSetterOnValue();


function TestArrayLengthSetter() {
  var a = [1];
  var o = {__proto__: a};
  o.length = 2;
  assertEquals(2, o.length);
  assertEquals(1, a.length);
  assertTrue(o.hasOwnProperty('length'));
}
TestArrayLengthSetter();


function TestArrayLengthSetterOnValue() {
  Number.prototype.__proto__ = [1];
  var n = 42;
  n.length = 2;
  assertEquals(1, n.length);
  assertFalse(Number.prototype.hasOwnProperty('length'));
}
TestArrayLengthSetterOnValue();
