// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

function foo() {
  return (new Uint8Array(10)).length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(10, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(10, foo());
assertOptimized(foo);

Object.defineProperty(Uint8Array.prototype.__proto__, "length",
                      { get() { return 42; } });

assertUnoptimized(foo);
assertEquals(42, foo());
