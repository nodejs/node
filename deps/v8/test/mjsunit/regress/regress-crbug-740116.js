// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestReflectGetPrototypeOfOnPrimitive() {
  function f() {
    return Reflect.getPrototypeOf('');
  };
  %PrepareFunctionForOptimization(f);
  assertThrows(f, TypeError);
  assertThrows(f, TypeError);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f, TypeError);
})();

(function TestObjectGetPrototypeOfOnPrimitive() {
  function f() {
    return Object.getPrototypeOf('');
  };
  %PrepareFunctionForOptimization(f);
  assertSame(String.prototype, f());
  assertSame(String.prototype, f());
  %OptimizeFunctionOnNextCall(f);
  assertSame(String.prototype, f());
})();

(function TestDunderProtoOnPrimitive() {
  function f() {
    return ''.__proto__;
  };
  %PrepareFunctionForOptimization(f);
  assertSame(String.prototype, f());
  assertSame(String.prototype, f());
  %OptimizeFunctionOnNextCall(f);
  assertSame(String.prototype, f());
})();
