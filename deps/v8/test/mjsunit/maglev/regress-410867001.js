// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-always-turbofan

function foo(a, b) {
  return a + b;
}
const v0 = '1';
const v1 = new String();
%PrepareFunctionForOptimization(foo);
foo(v0, v1);
%OptimizeMaglevOnNextCall(foo);
foo(v0, v1);
assertTrue(isMaglevved(foo));

// This will invalidate the StringWrapperToPrimitive protector.
Reflect.construct(v1.constructor, v1, Int16Array);
assertFalse(isMaglevved(foo));

v1.valueOf = () => ' 2';
assertEquals('1 2', foo(v0, v1));
