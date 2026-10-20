// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function __f_0() {
  try {
    try {
    } catch (__v_1) {}
    return Math.max;
  } catch (e) {}
}
function __f_1(__v_2, __v_4) {
  try {
    return __v_2();
  } catch (e) {}
}
function __f_2(__v_5, __v_6) {
  try {
    return __f_0() | __v_6 | __f_1(__f_0(), __v_5 | __v_6 | 0), undefined();
  } catch (e) {}
}

__f_2();
%PrepareFunctionForOptimization(__f_2);
__f_1();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
