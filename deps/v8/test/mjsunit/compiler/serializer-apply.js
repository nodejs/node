// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --turboshaft-enable-debug-features

function apply(arg) {
  "use strict";
  return arg.apply(this, arguments);
}

function bar() {
  function foo(self, arg) {
    %TurbofanStaticAssert(arg === 42);
    return %IsBeingInterpreted();
  }
  %PrepareFunctionForOptimization(foo);

  return apply(foo, 42);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(apply);
assertTrue(bar());
assertTrue(bar());
%OptimizeFunctionOnNextCall(bar);
assertFalse(bar());
