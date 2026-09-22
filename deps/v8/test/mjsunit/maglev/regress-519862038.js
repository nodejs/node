// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-truncated-int32-phis --turbolev-untagged-phis

function test() {
  for (let outer = 0; outer < 5; outer++) {
    let x = 1.1;
    let y = 4.07;
    for (let inner = 0; inner < 5; inner++) {
      // Truncating use of y
      let trunc = 1.1 >> y;
      x = y;
      y = Infinity;
    }
    global_val = x;
    %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(test);
test();
