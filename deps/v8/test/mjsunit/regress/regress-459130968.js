// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f, permissive = true) {
    return f();
}
  const __v_6 = __wrapTC(() => typeof __f_3);
function __f_9(__v_11) {
  const __v_13 = __wrapTC(() => __v_11 ?? -10);
  return __v_13 ^ __v_13;
}
function __f_10() {
  function __f_11() {
      const __v_14 = [ 107374182437134];
      __v_14.forEach(__f_9);
      for (let __v_15 = 0; __v_15 < 5; __v_15++) {
        __v_14.length = __v_15;
      }
  }
    %PrepareFunctionForOptimization(__f_11);
    %OptimizeMaglevOnNextCall(__f_11);
    __f_11();
}
  __f_10();
class __c_0 extends __f_10 {}
  new __c_0();
  new __c_0();
