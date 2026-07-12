// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --turbolev --turbolev-non-eager-inlining

function __wrapTC(f = true) {
  try {
    return f();
  } catch (e) {
  }
}
const __v_11 = () =>{};
function __f_6() {
  const __v_22 = __wrapTC(() => __v_24 => {
    try {
      "WnI".substring( __v_11);
      if (__v_21 != null && typeof __v_21 == "object") Object.defineProperty();
    } catch (e) {}
  });
    __v_22();
}
  %PrepareFunctionForOptimization(__f_6);
  __f_6();
  __f_6();
  %OptimizeFunctionOnNextCall(__f_6);
  __f_6();
