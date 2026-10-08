// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-concurrent-osr --no-turbo-loop-variable

// Regression test for crbug.com/502923583: Turbofan RepresentationChanger
// crash in the inlined Array.prototype.sort reduction.  Without loop variable
// analysis the outer loop Phi's type widens to Range(0, inf), gets kRepFloat64,
// and cannot be converted to kRepWord64 for use as a FixedArray index.

function main() {
  for (let i = 0; i < 5; i++) {
    %OptimizeOsr();
    [1, 2].sort((a, b) => i);
  }
}
%PrepareFunctionForOptimization(main);
main();
