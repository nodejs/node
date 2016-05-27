// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that traps that involve walking the target object's prototype chain
// don't overflow the stack when the original proxy is on that chain.

(function TestGetPrototype() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try { return p.__proto__; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestSetPrototype() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try { p.__proto__ = p; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestHasProperty() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try {
    return Reflect.has(p, "foo");
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestSet() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try { p.foo = 1; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestGet() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try { return p.foo; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestEnumerate() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  try { for (var x in p) {} } catch(e) { assertInstanceof(e, RangeError); }
})();

// The following traps don't involve the target object's prototype chain;
// we test them anyway for completeness.

(function TestIsExtensible() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  return Reflect.isExtensible(p);
})();

(function TestPreventExtensions() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  Reflect.preventExtensions(p);
})();

(function TestGetOwnPropertyDescriptor() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  return Object.getOwnPropertyDescriptor(p, "foo");
})();

(function TestDeleteProperty() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  delete p.foo;
})();

(function TestDefineProperty() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  Object.defineProperty(p, "foo", {value: "bar"});
})();

(function TestOwnKeys() {
  var p = new Proxy({}, {});
  p.__proto__ = p;
  return Reflect.ownKeys(p);
})();

(function TestCall() {
  var p = new Proxy(function() {}, {});
  p.__proto__ = p;
  return p();
})();

(function TestConstruct() {
  var p = new Proxy(function() { this.foo = 1; }, {});
  p.__proto__ = p;
  return new p();
})();
