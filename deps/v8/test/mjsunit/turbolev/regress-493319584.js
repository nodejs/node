// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --maglev-untagged-phis --no-lazy-feedback-allocation

// TODO(496266449): Re-enable --maglev-assert-types

function __f_2() {
  function __f_3() {
    try {
      // 0n cannot be converted to number, so this throws.
      return "number".charCodeAt(0n);
    } catch (__v_4) {}
    function __f_4() {
      eval();
      try {
      } catch (e) {}
      try {
      } catch (e) {}
    }
    // Cannot %PrepareFunctionForOptimization(__f_4) here; the bug won't repro.
    return __f_4;
  }
  %PrepareFunctionForOptimization(__f_3);
  const __v_3 = __f_3();
  __v_3();
}
%PrepareFunctionForOptimization(__f_2);
__f_2();
__f_2();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
