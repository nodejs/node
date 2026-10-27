// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --flag-processing-mode=ignore-contradictions --allow-natives-syntax
// Flags: --array-destructure-bytecode --no-maglev --no-turbolev

function test() {
  let a, b;
  [a, b] = [1, 2];
  return a + b;
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
