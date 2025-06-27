// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

assertTrue(%StringWrapperToPrimitiveProtector());

function createStringWrapper(newTarget) {
  return Reflect.construct(String, ["content"], newTarget);
}

%PrepareFunctionForOptimization(createStringWrapper);
createStringWrapper(String);
%OptimizeFunctionOnNextCall(createStringWrapper);

class X {
  [Symbol.toPrimitive]() {
    return 'new';
  }
}

assertTrue(%StringWrapperToPrimitiveProtector());

let x = createStringWrapper(X);

// The protector is also invalidated when we call Reflect.construct from optimized code.
assertFalse(%StringWrapperToPrimitiveProtector());

assertEquals('got new', 'got ' + x);
