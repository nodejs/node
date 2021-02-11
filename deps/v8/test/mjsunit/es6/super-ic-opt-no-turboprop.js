// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --super-ic --opt
// Flags: --no-always-opt --no-stress-opt --deopt-every-n-times=0

// This file contains tests which are disabled for TurboProp. TurboProp deopts
// differently than TurboFan, so the assertions about when a function is
// deoptimized won't hold.

(function TestPropertyIsConstant() {
  // Test for a case where the property is a constant found in the lookup start
  // object.
  class A {}
  A.prototype.bar = "wrong value: A.prototype.bar";

  class B extends A {};
  B.prototype.bar = "correct value";

  class C extends B {
    foo() { return super.bar; }
  }
  C.prototype.bar = "wrong value: C.prototype.bar";
  %PrepareFunctionForOptimization(C.prototype.foo);

  let o = new C();
  o.bar = "wrong value: o.bar";

  // Fill in the feedback.
  r = o.foo();
  assertEquals("correct value", r);

  %OptimizeFunctionOnNextCall(C.prototype.foo);

  // Test the optimized function.
  r = o.foo();
  assertEquals("correct value", r);

  // Assert that the function was not deoptimized.
  assertOptimized(C.prototype.foo);

  // Change the property value.
  B.prototype.bar = "new value";
  r = o.foo();
  assertEquals("new value", r);

  // Assert that the function was deoptimized (dependency to the constant
  // value).
  assertFalse(isOptimized(C.prototype.foo));
})();
