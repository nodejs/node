// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reflect.
(function () {
  // No trap.

  var target = {};
  var handler = {};
  var proxy = new Proxy(target, handler);

  assertTrue(Reflect.isExtensible(target));
  assertTrue(Reflect.isExtensible(proxy));
  assertTrue(Reflect.preventExtensions(proxy));
  assertFalse(Reflect.isExtensible(target));
  assertFalse(Reflect.isExtensible(proxy));
})();

(function () {
  // "Undefined" trap.

  var target = {};
  var handler = { preventExtensions: null };
  var proxy = new Proxy(target, handler);

  assertTrue(Reflect.isExtensible(target));
  assertTrue(Reflect.isExtensible(proxy));
  assertTrue(Reflect.preventExtensions(proxy));
  assertFalse(Reflect.isExtensible(target));
  assertFalse(Reflect.isExtensible(proxy));
})();


(function () {
  // Invalid trap.

  var target = {};
  var handler = { preventExtensions: 42 };
  var proxy = new Proxy(target, handler);

  assertThrows(() => { Reflect.preventExtensions(proxy) }, TypeError);
})();


(function () {
  var target = {};
  var handler = { isExtensible() { return "bla" } };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and target is extensible.
  assertTrue(Reflect.isExtensible(proxy));

  // Trap returns trueish but target is not extensible.
  Reflect.preventExtensions(target);
  assertThrows(() => { Reflect.isExtensible(proxy) }, TypeError);
})();


(function () {
  // Trap returns falsish.

  var target = {};
  var handler = { preventExtensions() { return 0 } };
  var proxy = new Proxy(target, handler);

  assertFalse(Reflect.preventExtensions(proxy));
  Reflect.preventExtensions(target);
  assertFalse(Reflect.preventExtensions(proxy));
})();


(function () {
  var target = {};
  var handler = { preventExtensions() { return Symbol() } };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish but target is extensible.
  assertThrows(() => { Reflect.preventExtensions(proxy) }, TypeError);

  // Trap returns trueish and target is not extensible.
  Reflect.preventExtensions(target);
  assertTrue(Reflect.preventExtensions(proxy));
})();


(function () {
  // Target is proxy
  var object = {};
  assertTrue(Reflect.preventExtensions(object));
  var target = new Proxy(object, {});
  var proxy = new Proxy(target, {});
  assertFalse(Reflect.isExtensible(object));
  assertFalse(Reflect.isExtensible(target));
  assertFalse(Reflect.isExtensible(proxy));
})();

// Object.
(function () {
  // No trap.

  var target = {};
  var handler = {};
  var proxy = new Proxy(target, handler);

  assertTrue(Object.isExtensible(target));
  assertTrue(Object.isExtensible(proxy));
  assertSame(proxy, Object.preventExtensions(proxy));
  assertFalse(Object.isExtensible(target));
  assertFalse(Object.isExtensible(proxy));
})();

(function () {
  // "Undefined" trap.

  var target = {};
  var handler = { preventExtensions: null };
  var proxy = new Proxy(target, handler);

  assertTrue(Object.isExtensible(target));
  assertTrue(Object.isExtensible(proxy));
  assertSame(proxy, Object.preventExtensions(proxy));
  assertFalse(Object.isExtensible(target));
  assertFalse(Object.isExtensible(proxy));
})();


(function () {
  // Invalid trap.

  var target = {};
  var handler = { preventExtensions: 42 };
  var proxy = new Proxy(target, handler);

  assertThrows(() => { Object.preventExtensions(proxy) }, TypeError);
})();


(function () {
  var target = {};
  var handler = { isExtensible() { return "bla" } };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and target is extensible.
  assertTrue(Object.isExtensible(proxy));

  // Trap returns trueish but target is not extensible.
  assertSame(target, Object.preventExtensions(target));
  assertThrows(() => { Object.isExtensible(proxy) }, TypeError);
})();


(function () {
  // Trap returns falsish.

  var target = {};
  var handler = { isExtensible() { return false } };
  var proxy = new Proxy(target, handler);

  assertThrows(() => { Object.isExtensible(proxy) }, TypeError);
  assertSame(target, Object.preventExtensions(target));
  assertFalse(Object.isExtensible(proxy));
})();


(function () {
  // Trap returns falsish.

  var target = {};
  var handler = { preventExtensions() { return 0 } };
  var proxy = new Proxy(target, handler);

  assertFalse(Reflect.preventExtensions(proxy));
  assertSame(target, Object.preventExtensions(target));
  assertFalse(Reflect.preventExtensions(proxy));
  assertThrows(() => { Object.preventExtensions(proxy) }, TypeError);
})();


(function () {
  var target = {};
  var handler = { preventExtensions() { return Symbol() } };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish but target is extensible.
  assertThrows(() => { Object.preventExtensions(proxy) }, TypeError);

  // Trap returns trueish and target is not extensible.
  assertSame(target, Object.preventExtensions(target));
  assertTrue(Reflect.preventExtensions(proxy));
})();


(function () {
  // Target is proxy
  var object = {};
  assertSame(object, Object.preventExtensions(object));
  var target = new Proxy(object, {});
  var proxy = new Proxy(target, {});
  assertFalse(Object.isExtensible(object));
  assertFalse(Object.isExtensible(target));
  assertFalse(Object.isExtensible(proxy));
})();
