// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --flush-denormals

function divByPow2(x) {
  return x / 8.98846567431158e+307; // 2^1023
}

%PrepareFunctionForOptimization(divByPow2);
for (let i = 0; i < 100; i++) divByPow2(2.0);
%OptimizeFunctionOnNextCall(divByPow2);

let optimized = divByPow2(2.0);
let expected = 2.0 / 8.98846567431158e+307;

assertEquals(expected, optimized);
