// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt

function foo(a, b) {
    return a - b;
}

%PrepareFunctionForOptimization(foo);
assertEquals(-1n, foo(1n, 2n));
%OptimizeFunctionOnNextCall(foo);
assertEquals(1n, foo(2n, 1n));
assertOptimized(foo);
assertThrows(() => foo(2n, undefined));
assertUnoptimized(foo);
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertEquals(-1n, foo(1n, 2n));
assertOptimized(foo);
assertThrows(() => foo(undefined, 2n));
assertOptimized(foo);
