// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboprop --gc-interval=1000

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

function foo() {
  runNearStackLimit(() => {});
}

(function () {
  var a = 42;
  var b = 153;
  try {
    Object.defineProperty({});
  } catch (e) {}
    foo();
    foo();
})();

runNearStackLimit(() => {});
