// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=100

// Test that traps that involve walking the target object's prototype chain
// don't overflow the stack when the original proxy is on that chain.

(function TestGetPrototype() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { return p.__proto__; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestSetPrototype() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { p.__proto__ = p; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestHasProperty() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    return Reflect.has(p, "foo");
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestSet() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { p.foo = 1; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestGet() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { return p.foo; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestEnumerate() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { for (var x in p) {} } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestIsExtensible() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    return Reflect.isExtensible(p);
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestPreventExtensions() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    Reflect.preventExtensions(p);
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestGetOwnPropertyDescriptor() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    return Object.getOwnPropertyDescriptor(p, "foo");
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestDeleteProperty() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try { delete p.foo; } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestDefineProperty() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    Object.defineProperty(p, "foo", {value: "bar"});
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestOwnKeys() {
  var handler = {};
  var p = new Proxy({}, handler);
  handler.__proto__ = p;
  try {
    return Reflect.ownKeys(p);
  } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestCall() {
  var handler = {};
  var p = new Proxy(function() {}, handler);
  handler.__proto__ = p;
  try { return p(); } catch(e) { assertInstanceof(e, RangeError); }
})();

(function TestConstruct() {
  var handler = {};
  var p = new Proxy(function() { this.foo = 1; }, handler);
  handler.__proto__ = p;
  try { return new p(); } catch(e) { assertInstanceof(e, RangeError); }
})();
