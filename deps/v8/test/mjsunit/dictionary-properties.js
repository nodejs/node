// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test loading existent and nonexistent properties from dictionary
// mode objects.

function SlowObject() {
  this.foo = 1;
  this.bar = 2;
  this.qux = 3;
  this.z = 4;
  delete this.qux;
  assertFalse(%HasFastProperties(this));
}
function SlowObjectWithBaz() {
  var o = new SlowObject();
  o.baz = 4;
  return o;
}

function Load(o) {
  return o.baz;
}

for (var i = 0; i < 10; i++) {
  var o1 = new SlowObject();
  var o2 = SlowObjectWithBaz();
  assertEquals(undefined, Load(o1));
  assertEquals(4, Load(o2));
}

// Test objects getting optimized as fast prototypes.

function SlowPrototype() {
  this.foo = 1;
}
SlowPrototype.prototype.bar = 2;
SlowPrototype.prototype.baz = 3;
SlowPrototype.prototype.z = 4;
delete SlowPrototype.prototype.baz;
assertFalse(%HasFastProperties(SlowPrototype.prototype));
var slow_proto = new SlowPrototype;
// ICs make prototypes fast.
function ic() { return slow_proto.bar; }
ic();
ic();
assertEquals(!%IsDictPropertyConstTrackingEnabled(),
             %HasFastProperties(slow_proto.__proto__));

// Prototypes stay fast even after deleting properties.
assertEquals(!%IsDictPropertyConstTrackingEnabled(),
             %HasFastProperties(SlowPrototype.prototype));
var fast_proto = new SlowPrototype();
assertEquals(!%IsDictPropertyConstTrackingEnabled(),
             %HasFastProperties(SlowPrototype.prototype));
assertEquals(!%IsDictPropertyConstTrackingEnabled(),
             %HasFastProperties(fast_proto.__proto__));


if (!%IsDictPropertyConstTrackingEnabled()) {
  assertTrue(%HasFastProperties(SlowPrototype.prototype));
  assertTrue(%HasFastProperties(fast_proto.__proto__));
}
