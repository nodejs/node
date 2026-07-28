// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

__f_5(7, 7);
function __f_3() {
  try {
    for (let __v_8 = 0; __v_8 < 5; __v_8++) {
    }
  } catch (e) {}
  return Math.max;
}
function __f_4(__v_9, __v_10, __v_11) {
  return __v_9(__v_10, __v_11);
}
function __f_5(__v_12, __v_13) {
  return __f_4(__f_3(), __v_12 | 0, __v_13 | 0);
}
%PrepareFunctionForOptimization(__f_5);
try {
  __f_4();
} catch (e) {}
%OptimizeFunctionOnNextCall(__f_5);
__f_5();
