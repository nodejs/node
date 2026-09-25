// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}
const __v_16 = () => new __v_15();
const __v_17 = () => new __v_15();
function __f_5(__v_18) {
  try {
    __v_18.toString = __f_5;
  } catch (__v_29) {}
  const __v_21 = __wrapTC();
  const __v_22 = __wrapTC(() => __v_18.toString);
  try {
    __v_23 = __v_22();
    __v_23 = __v_22();
  } catch (e) {}
  const __v_26 = __wrapTC(() => 0 .toLocaleString);
  const __v_27 = __wrapTC(() => __v_26.constructor);
}
%PrepareFunctionForOptimization(__f_5);
__f_5(__v_17);
__f_5(__v_16);
%OptimizeFunctionOnNextCall(__f_5);
__f_5(__v_16);
__f_5();
