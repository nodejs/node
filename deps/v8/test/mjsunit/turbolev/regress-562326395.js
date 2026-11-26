// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan
// Flags: --turbolev-non-eager-loop-peeling
// Flags: --maglev-loop-peeling-max-size-cumulative=35

function test(n) {
  const lim = n | 0;
  for (let i = 0; i < lim; i++) {
    %AssertPeeled();
  }
  for (let j = 0; j < lim; j++) {
    %AssertNotPeeled();
  }
  return lim;
}

%PrepareFunctionForOptimization(test);
assertEquals(10, test(10));
assertEquals(10, test(10));
%OptimizeFunctionOnNextCall(test);
assertEquals(0, test(0));
assertEquals(10, test(10));
assertOptimized(test);
