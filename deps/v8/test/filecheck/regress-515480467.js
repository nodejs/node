// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --print-all-exceptions --no-concurrent-recompilation --turbofan

// CHECK: JS stack trace
// CHECK: {{[0-9]+}}: mul

let worker_code = `
  onmessage = function(e) {
    function mul(a, b) {
      return a * b;
    }
    let large = 2n ** 100n;
    %PrepareFunctionForOptimization(mul);
    mul(large, large);
    %OptimizeFunctionOnNextCall(mul);
    mul(large, large); // triggers optimization

    // Assert that mul is optimized
    if ((%GetOptimizationStatus(mul) & 8) === 0) {
      throw new Error("mul was not optimized!");
    }

    let x = (2n ** 8000000n) - 1n;
    let y = (2n ** 8000000n) - 1n;
    postMessage("starting");
    mul(x, y);
  };
`;
let w = new Worker(worker_code, {type: 'string'});
w.postMessage("start");
let msg = w.getMessage();
w.terminate();
