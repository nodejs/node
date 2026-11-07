// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --array-destructure-bytecode

function test() {
  let a, b;
  [a, b] = [1, 2];
  return a + b;
}

%PrepareFunctionForOptimization(test);
assertEquals(3, test());
%OptimizeFunctionOnNextCall(test);
assertEquals(3, test());
