// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --no-always-turbofan
// Flags: --typed-array-length-loading

Object.defineProperty(Uint8Array.prototype.__proto__, "length",
                      { get() { return 42; } });

function foo() {
  return (new Uint8Array()).length;
}
%PrepareFunctionForOptimization(foo);

assertEquals(42, foo());

%OptimizeFunctionOnNextCall(foo);
assertEquals(42, foo());
assertOptimized(foo);
