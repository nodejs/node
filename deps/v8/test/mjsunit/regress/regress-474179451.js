// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing --maglev-non-eager-inlining --max-maglev-inlined-bytecode-size-small=0

function __f_14() {
    let __v_210 = 0;
    let __v_211 = -1;
    (function __f_46() {
      let __v_212 = -1;
      function __f_47() {
        __v_211;
        __v_212;
        __v_210 = 1;
      }
      return __f_47;
    })()();
    return __v_210;
}
%PrepareFunctionForOptimization(__f_14);
__f_14();
__f_14();
%OptimizeMaglevOnNextCall(__f_14);
assertEquals(1, __f_14());
