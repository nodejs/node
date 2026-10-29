// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


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
  var handler = { isExtensible: null };
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
  var handler = { isExtensible: true };
  var proxy = new Proxy(target, handler);

  assertThrows(() => {Reflect.isExtensible(proxy)}, TypeError);
})();


(function () {
  var target = {};
  var handler = { isExtensible() {return "bla"} };
  var proxy = new Proxy(target, handler);

  // Trap returns trueish and target is extensible.
  assertTrue(Reflect.isExtensible(proxy));

  // Trap returns trueish but target is not extensible.
  Reflect.preventExtensions(target);
  assertThrows(() => {Reflect.isExtensible(proxy)}, TypeError);
})();


(function () {
  var target = {};
  var handler = { isExtensible() {return 0} };
  var proxy = new Proxy(target, handler);

  // Trap returns falsish but target is extensible.
  assertThrows(() => {Reflect.isExtensible(proxy)}, TypeError);

  // Trap returns falsish and target is not extensible.
  Reflect.preventExtensions(target);
  assertFalse(Reflect.isExtensible(proxy));
})();
