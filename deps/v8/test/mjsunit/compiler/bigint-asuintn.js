// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt


function f(x) {
  return BigInt.asUintN(3, x);
}

%PrepareFunctionForOptimization(f);
assertEquals(7n, f(7n));
assertEquals(1n, f(9n));
%OptimizeFunctionOnNextCall(f);
assertEquals(7n, f(7n));
assertEquals(1n, f(9n));
assertOptimized(f);

// BigInt.asUintN throws TypeError for non-BigInt arguments.
assertThrows(() => f(2), TypeError);
if(%Is64Bit()) {
  // On 64 bit architectures TurboFan optimizes BigInt.asUintN to native code
  // that deoptimizes on non-BigInt arguments.
  assertUnoptimized(f);

  // The next time the function is optimized, speculation should be disabled
  // so the builtin call is kept, which won't deoptimize again.
  %PrepareFunctionForOptimization(f);
  %OptimizeFunctionOnNextCall(f);
}
assertEquals(7n, f(7n));
assertOptimized(f);

// Re-optimized still throws but does not deopt-loop.
assertThrows(() => f(2), TypeError);
assertOptimized(f);
