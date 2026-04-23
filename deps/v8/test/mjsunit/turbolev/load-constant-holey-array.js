// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function foo(a) {
  const arr = [0, 1, 2, /*hole*/, a];
  return arr[1];
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo(5));
assertEquals(1, foo(5));

%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo(5));
