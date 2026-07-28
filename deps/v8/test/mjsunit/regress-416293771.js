// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __wrapTC(f) {
  return f();
}

function __f_8(__v_18) {
let __v_19 = __wrapTC(() => Math.max((__v_18 | 0) * 2 ** 52, 4294967297));
  __v_19--;
return __v_19 + __v_19;
}
%PrepareFunctionForOptimization(__f_8);
__f_8();
%OptimizeFunctionOnNextCall(__f_8);
function __f_9() {
  __f_8(1);
}
%PrepareFunctionForOptimization(__f_9);
__f_9();
%OptimizeFunctionOnNextCall(__f_9);
__f_9();
