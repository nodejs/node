// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --turboshaft-enable-debug-features

var expect_interpreted = true;

class C {
  get prop() {
    return 42;
  }
  set prop(v) {
    if (!%IsDictPropertyConstTrackingEnabled()) {
      // TODO(v8:11457) If v8_dict_property_const_tracking is enabled, then
      // C.prototype is a dictionary mode object and we cannot inline the call
      // to this setter, yet.
      assertEquals(expect_interpreted, %IsBeingInterpreted());
    }
    %TurbofanStaticAssert(v === 43);
  }
}

const c = new C();

function foo() {
  assertEquals(expect_interpreted, %IsBeingInterpreted());
  %TurbofanStaticAssert(c.prop === 42);
  c.prop = 43;
}

%PrepareFunctionForOptimization(
    Object.getOwnPropertyDescriptor(C.prototype, 'prop').get);
%PrepareFunctionForOptimization(
    Object.getOwnPropertyDescriptor(C.prototype, 'prop').set);
%PrepareFunctionForOptimization(foo);

foo();
foo();
expect_interpreted = false;
%OptimizeFunctionOnNextCall(foo);
foo();
