// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const x = (-1).toLocaleString().padEnd(2 + 31 - 1, 'aa');

function test() {
  try {
    y = x;
  } catch (e) {
  }
  return y > -Infinity ? y : 0 - y;
}

%PrepareFunctionForOptimization(test);
assertEquals(NaN, test());
%OptimizeFunctionOnNextCall(test);
assertEquals(NaN, test());
