// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --allow-natives-syntax --turbolev

for (let __v_36 = 0; __v_36 < 5; __v_36++) {
  async function __f_16() {
    let __v_38 = undefined;
    function __f_17() {
      if (__v_38 != null && typeof __v_38 == "object") Object.defineProperty();
    }
    try {
      return __f_15;
      function __f_18() {
        return __f_18;
      }
    } catch (__v_39) {
      await 0;
    }
    __v_38 = "bar";
  }
  const __v_37 = %OptimizeFunctionOnNextCall(__f_16);
  __f_16();
}
