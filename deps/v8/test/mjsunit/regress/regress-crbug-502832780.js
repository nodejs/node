// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing

// Regression test for crbug.com/502832780: MaglevGraphVerifier crash on a Phi
// with a null input, caused by the inlined Array.prototype.sort reduction not
// handling the abort path correctly when the comparefn's return value cannot be
// converted to float64 (e.g. when comparefn returns a JSFunction).

function main() {
  for (let i = 0; i < 5; i++) {
    const v0 = [];
    function f1() {
      return f1;
    }
    Math.max(f1);
    v0.sort(f1);
    %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(main);
main();
