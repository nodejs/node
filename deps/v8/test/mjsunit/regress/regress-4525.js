// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function receiver() {
  return this;
}

function construct(f) {
  "use strict";
  class B {}
  class C extends B {
    bar() { return super.foo() }
  }
  B.prototype.foo = f;
  return new C();
}

function check(x, value, type) {
  assertEquals("object", typeof x);
  assertInstanceof(x, type);
  assertEquals(value, x);
}

var o = construct(receiver);
check(o.bar.call(123), Object(123), Number);
check(o.bar.call("a"), Object("a"), String);
check(o.bar.call(undefined), this, Object);
check(o.bar.call(null), this, Object);

%OptimizeFunctionOnNextCall(o.bar);
check(o.bar.call(456), Object(456), Number);
check(o.bar.call("b"), Object("b"), String);
check(o.bar.call(undefined), this, Object);
check(o.bar.call(null), this, Object);
