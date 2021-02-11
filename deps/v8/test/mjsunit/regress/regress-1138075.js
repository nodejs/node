// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --max-semi-space-size=1

function runNearStackLimit(f) {
  function t() {
    try {
      return t();
    } catch (e) {
      return f();
    }
  }
  %PrepareFunctionForOptimization(t);
  %OptimizeFunctionOnNextCall(t);
  return t();
}

function foo(a) {}
function bar(a, b) {}

for (let i = 0; i < 150; i++) {
  runNearStackLimit(() => {
    return foo(bar(3, 4) === false);
  });
}
