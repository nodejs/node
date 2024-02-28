// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft --turboshaft-load-elimination --jit-fuzzing
// Flags: --allow-natives-syntax

function f0() {
  const v3 = [2.0, , {}];
  function f5(a7) {
    a7.join().concat(a7);
    ~a7[0];
    const o12 = {
      __proto__: v3,
    };
  }
  f5(v3);
}
%PrepareFunctionForOptimization(f0);
f0();
%OptimizeFunctionOnNextCall(f0);
f0();
