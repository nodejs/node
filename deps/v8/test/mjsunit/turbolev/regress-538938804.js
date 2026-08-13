// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --no-lazy-feedback-allocation --no-baseline-batch-compilation

let counter = 0;
let mismatch = 0;

function inc() {
  counter = counter + 1;
}

while (counter < 10) {
  function foo() {
    var snapshot = counter;
    for (var i = 0; i < 2; ++i) {
      for (var j = 1; j < 3; ++j) {
        inc();
        if (snapshot + j != counter) {
          mismatch = counter;
        }
      }
      snapshot = counter;
    }
  }
  %PrepareFunctionForOptimization(foo);
  foo();
  %OptimizeFunctionOnNextCall(foo);
}

assertEquals(0, mismatch);
