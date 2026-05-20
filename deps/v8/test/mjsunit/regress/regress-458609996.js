// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_1(__v_2, __v_3, __v_4) {
  try {
    __v_2--;
    const __v_5 = Array(  __v_4);
      __v_5.findLastIndex(__v_3);
    Array(__v_2, __v_4);
  } catch (e) {}
}
for (let __v_9 = 0; __v_9 < 5; __v_9++) {
    __f_1(__v_9, __f_1);
    %PrepareFunctionForOptimization(__f_1);
    %OptimizeFunctionOnNextCall(__f_1);
}
