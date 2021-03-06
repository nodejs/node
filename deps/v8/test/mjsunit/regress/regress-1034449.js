// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt
// Flags: --no-stress-flush-bytecode

function f(len) {
  return new Array(len);
}

%PrepareFunctionForOptimization(f);
assertEquals(3, f(3).length);
assertEquals(18, f(18).length);
%OptimizeFunctionOnNextCall(f);
assertEquals(4, f(4).length);
assertOptimized(f);
let a = f("8");
assertUnoptimized(f);
assertEquals(1, a.length);
assertEquals("8", a[0]);

// Check there is no deopt loop.
%PrepareFunctionForOptimization(f);
assertEquals(1, f(1).length);
%OptimizeFunctionOnNextCall(f);
assertEquals(8, f(8).length);
assertOptimized(f);
assertEquals(1, f("8").length);
assertOptimized(f);
