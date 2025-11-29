// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var target = { target: 1 };
target.__proto__ = {};
var handler = { handler: 1 };
var proxy = new Proxy(target, handler);

assertSame(Object.getPrototypeOf(proxy), target.__proto__ );

target.__proto__ = [];
assertSame(Object.getPrototypeOf(proxy), target.__proto__);

handler.getPrototypeOf = function() {
  return 1;
}
assertThrows(function() { Object.getPrototypeOf(proxy) }, TypeError);

var target_prototype = {a:1, b:2};
handler.getPrototypeOf = function() {
  return target_prototype ;
}
assertSame(Object.getPrototypeOf(proxy), target_prototype);

// Test with proxy target:
var proxy2 = new Proxy(proxy, {'handler':1});
assertSame(Object.getPrototypeOf(proxy2), target_prototype);

// Test with Proxy handler:
var proxy3_prototype = {'proto3':true};
var handler_proxy = new Proxy({
  getPrototypeOf: function() { return proxy3_prototype }
}, {});
var proxy3 = new Proxy(target, handler_proxy);
assertSame(Object.getPrototypeOf(proxy3), proxy3_prototype);


// Some tests with Object.prototype.isPrototypeOf

(function () {
  var object = {};
  var handler = {};
  var proto = new Proxy({}, handler);
  object.__proto__ = proto;

  assertTrue(proto.isPrototypeOf(object));
  assertTrue(Object.prototype.isPrototypeOf.call(proto, object));

  handler.getPrototypeOf = function () { return Object.prototype };
  assertTrue(proto.isPrototypeOf(object));
  assertTrue(Object.prototype.isPrototypeOf.call(proto, object));
  assertTrue(Object.prototype.isPrototypeOf(object));
  assertFalse(Object.prototype.isPrototypeOf.call(Array.prototype, object));
  assertFalse(Array.prototype.isPrototypeOf(object));

  handler.getPrototypeOf = function () { return object };
  assertTrue(Object.prototype.isPrototypeOf.call(proto, object));
  assertTrue(proto.isPrototypeOf(object));
  assertTrue(Object.prototype.isPrototypeOf.call(object, object));
  assertTrue(object.isPrototypeOf(object));

  handler.getPrototypeOf = function () { throw "foo" };
  assertTrue(proto.isPrototypeOf(object));
  assertTrue(Object.prototype.isPrototypeOf.call(proto, object));
  assertThrows(()=> Object.prototype.isPrototypeOf(object));
  assertThrows(()=> Object.prototype.isPrototypeOf.call(Array.prototype, object));
  assertThrows(()=> Array.prototype.isPrototypeOf(object));
})();

(function () {
  var handler = {};
  var object = new Proxy({}, handler);
  var proto = {};

  assertFalse(Object.prototype.isPrototypeOf.call(object, object));
  assertFalse(Object.prototype.isPrototypeOf.call(proto, object));
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, object));

  handler.getPrototypeOf = function () { return proto };
  assertTrue(Object.prototype.isPrototypeOf.call(proto, object));
  assertFalse(Object.prototype.isPrototypeOf.call({}, object));
  assertTrue(Object.prototype.isPrototypeOf.call(Object.prototype, object));

  handler.getPrototypeOf = function () { return object };
  assertTrue(Object.prototype.isPrototypeOf.call(object, object));

  handler.getPrototypeOf = function () { throw "foo" };
  assertThrows(()=> Object.prototype.isPrototypeOf.call(object, object));
  assertThrows(()=> Object.prototype.isPrototypeOf(object));
})();
