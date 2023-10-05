// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --turboshaft-enable-debug-features

function call(cb) {
  return cb.call(this, 42);
}

function bar() {
  function foo(arg) {
    %TurbofanStaticAssert(arg === 42);
    return %IsBeingInterpreted();
  }
  %PrepareFunctionForOptimization(foo);

  return call(foo);
}


%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(call);
assertTrue(bar());
assertTrue(bar());
%OptimizeFunctionOnNextCall(bar);
assertFalse(bar());
