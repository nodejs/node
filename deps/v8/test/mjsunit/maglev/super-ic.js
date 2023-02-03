// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

class A {
    foo() {
        return 42;
    }
};
class B extends A {
  bar() {
    return super.foo();
  }
};
class C {
  foo() {
      return 24;
  }
};

var o = new B();

%PrepareFunctionForOptimization(o.bar);
assertEquals(o.bar(), 42);

%OptimizeMaglevOnNextCall(o.bar);
assertEquals(o.bar(), 42);
assertTrue(isMaglevved(o.bar));

// This should deopt.
o.__proto__.__proto__ = new C();
assertEquals(o.bar(), 24);
assertFalse(isMaglevved(o.bar));
