// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

// This test should crash when ran with Turbofan/Turbolev because the static
// assert doesn't hold. The reason that this test exists is to help us realize
// if %TurbofanStaticAssert starts being silently ignored by Turbolev: if this
// happens, this test will unexpectedly pass and we'll notice.

function foo(n) {
  %TurbofanStaticAssert(n+1 == n);
}

%PrepareFunctionForOptimization(foo);
foo(42);

%OptimizeFunctionOnNextCall(foo);
foo(42);
assertOptimized(foo);
