// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}
function __f_0(__v_12, __v_13) {
  let __v_14 = __wrapTC(() => Math.atan(0 * 0 / __v_13));
  return __v_14--;
}
const __v_7 = __wrapTC(() => %PrepareFunctionForOptimization(__f_0));
__f_0(0, 1);
const __v_8 = __wrapTC(() => %OptimizeFunctionOnNextCall(__f_0));
__f_0();
