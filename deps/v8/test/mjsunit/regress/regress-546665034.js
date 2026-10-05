// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function EnsureDictionaryMode(obj) {
  obj.foo = 0;
  obj.bar = 0;
  // Delete the second-to-last property first to force normalization.
  delete obj.foo;
  delete obj.bar;
  assertFalse(%HasFastProperties(obj));
}

var n = 0x100000000;
var rab;
try {
  rab = new ArrayBuffer(n + 1, { maxByteLength: n + 1 });
} catch(e) {
  // It must be a "RangeError: Invalid array buffer length" on 32-bit
  // configurations. Skip it.
  quit(0);
}
var ta = new Int8Array(rab);

// Test stores to an indexed property with a big index while there's
// a typed array in the prototype chain.

(function TestFastModeDataProperty() {
  function A() {}
  var obj = new A();
  Object.setPrototypeOf(obj, ta);
  obj[n] = 42;
})();

(function TestSlowModeDataProperty() {
  function A() {}
  var obj = new A();
  EnsureDictionaryMode(obj);
  Object.setPrototypeOf(obj, ta);
  obj[n] = 42;
})();

// Put this case to the end since it modifies the Object.prototype.
(function TestGlobalObjectDataProperty() {
  var globalProto = Object.getPrototypeOf(globalThis);
  Object.setPrototypeOf(globalProto, ta);
  globalThis[n] = 42;
})();
