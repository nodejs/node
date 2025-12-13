// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --stress-concurrent-inlining
// Flags: --turbolev

var __v_4 = new DataView(new ArrayBuffer());
function __f_4(__v_7, __v_8, __v_9) {
  try {
    var __v_10 = new Array(__v_8);
    var __v_11 = new __v_7();
    for (var __v_12 = 0; __v_12 < __v_8; ++__v_12) {
      __v_11[__v_12] = __v_10[__v_12] = __v_9 === false ? __v_4 : __v_8 % __v_9;
    }
    var __v_13 = new __v_4();
  } catch (e) {}
}
%PrepareFunctionForOptimization(__f_4);
var __v_16 = 1;
__f_4(Uint32Array, __v_16, Math.pow());
%OptimizeFunctionOnNextCall(__f_4);
__f_4();
