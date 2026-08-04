// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f = true) {
    return f();
}
function __f_3(__v_36, __v_37) {
  function __f_4(__v_38, __v_39, __v_40) {
    try {
      const __v_42 = [/* NumberMutator: Replaced 127 with 134 */134, 2, 4294967295, 268435456, 1754102230, -536870912];
      __v_42[143] = 268435456;
      const __v_43 = __v_42[10] ?? __v_40;
      __v_43 >> __v_43;
      return 268435456;
    } catch (e) {}
  }
    __f_4(__v_37, __f_3, __f_4());
}
const __v_35 = __wrapTC(() => __f_3());
  for (let __v_44 = 0; __v_44 < 5; __v_44++) {
    __f_3();
    %PrepareFunctionForOptimization(__f_3);
    %OptimizeMaglevOnNextCall(__f_3);
  }
