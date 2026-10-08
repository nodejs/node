// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --allow-natives-syntax  --optimize-on-next-call-optimizes-to-maglev --jit-fuzzing

class __c_0 {}
function* __f_5() {
    for (let __v_19 = 0; __v_19 < 5; __v_19++) {
        eval();
        yield 6;
        SharedArrayBuffer.prototype;
    }
}
  for (const __v_22 of (/* FunctionCallMutator: Optimizing __f_5 */%PrepareFunctionForOptimization(), __f_5(), __f_5(), %OptimizeFunctionOnNextCall(__f_5), __f_5())) {}
