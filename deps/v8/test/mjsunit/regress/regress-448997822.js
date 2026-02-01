// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_1() {
    const __v_0 = [];
    __v_0[8] = 4.2;
    for (let __v_1 = 0; __v_1 < 5; __v_1++) {
      let __v_2 = __v_0[4];
      for (let __v_3 = 0; __v_3 < 5; __v_3++) {
        __v_1 = __v_2;
        try {
          __v_2.n();
        } catch (__v_4) {}
        ++__v_2;
      }
    }
}
  %PrepareFunctionForOptimization(__f_1);
  __f_1();
  %OptimizeFunctionOnNextCall(__f_1);
  __f_1();
