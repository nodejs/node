// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function __f_1(__v_9) {
    %PrepareFunctionForOptimization(__v_9);
    __v_9();
    %OptimizeFunctionOnNextCall(__v_9);
 __v_9();
}
function __f_6() {
  try {
    return Math.max(Math.__v_16 |  1010);
  } catch (e) {}
}
function __f_7() {
  return __f_6() * -127;
}
__f_1(__f_7, -254);
