// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --assert-types

function __f_2(__v_4, __v_5) {
  let __v_6 = __v_4 >= __v_5;
    while (__v_6 != 0) {
        __v_4 = __v_4 | __v_5 - __v_4;
      let __v_7 = __v_4 >= __v_5;
        new Int32Array(__v_4);
        __v_6 = __v_4 < __v_5;
    }
}
function __f_3() {
  __f_2(Infinity, 1);
    __f_2();
}
  %PrepareFunctionForOptimization(__f_3);
  %PrepareFunctionForOptimization(__f_2);
  __f_3();
  %OptimizeFunctionOnNextCall(__f_3);
  __f_3();
