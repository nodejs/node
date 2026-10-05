// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --fuzzing --maglev-non-eager-inlining --no-lazy-feedback-allocation

function __f_7() {
  try {
    return Array.prototype.at;
  } catch (e) {}
}
function __f_8(__v_27) {
    __v_27[0];
  return __f_7().call(__v_27, -6);
}
try {
} catch (e) {}
  %PrepareFunctionForOptimization(__f_8);
  __f_8([]);
  %OptimizeFunctionOnNextCall(__f_8);
  __f_8([]);
