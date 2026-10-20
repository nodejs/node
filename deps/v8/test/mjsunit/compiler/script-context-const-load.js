// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --bundle --turbofan

// JS_BUNDLE_SCRIPT
const scriptConst = 42;

// JS_BUNDLE_SCRIPT
function createClosure() {
  return function foo() {
    %TurbofanStaticAssert(scriptConst == 42);
    return scriptConst;
  };
}

// Create the closure twice to avoid function context specialization.
const f1 = createClosure();
const f2 = createClosure();

%PrepareFunctionForOptimization(f1);
assertEquals(42, f1());
%OptimizeFunctionOnNextCall(f1);
assertEquals(42, f1());
assertOptimized(f1);
