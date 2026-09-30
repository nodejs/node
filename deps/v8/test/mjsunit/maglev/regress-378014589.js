// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
  function F0() {}
  class C3 extends F0 {}
  for (let v4 = 0; v4 < 5; v4++) {
    for (let v5 = 0; v5 < 5; v5++) {
      const v6 = %OptimizeOsr();
    }
    let v7 = 0;
    do {
      function f8() {
        // Triggers deopt in inlinee
        Math[Math.round(-0.1)];
      }
      f8();
      v7++;
    } while ((() => {
      // Observes if v7 is correct.
      return v7 < 10;
    })())
  }
}

%PrepareFunctionForOptimization(test);
test();
