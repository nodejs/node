// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing
// Without --fuzzing, %PrepareFunctionForOptimization would fail.

for (let i = 0; i < 2; i++) {
  try { new this.invalid(); } catch {}

  function invalid(x) {
    "use asm";
    var y = x.Math.fround;
    function foo() {}
    return {foo: foo};
  }

  %PrepareFunctionForOptimization(invalid);
  %OptimizeFunctionOnNextCall(invalid);
}
