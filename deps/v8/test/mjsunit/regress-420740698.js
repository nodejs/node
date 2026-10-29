// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function runNearStackLimit(f) {
  try {
    return t();
  } catch (e) {
    return f();
  }
}
let lazy = () => fail();
(function () {
  async function foo() {
    for (let i = 0; i < 1; i++) {
      for (let j = 0; j < 4; j++) {
        lazy | 5;
        await 7;
      }
    }
  }
  %PrepareFunctionForOptimization(foo);
  try {
  runNearStackLimit(() => {
    return foo();
  });
  } catch {};
  %OptimizeFunctionOnNextCall(foo);
  try { foo(); } catch {};
})();
