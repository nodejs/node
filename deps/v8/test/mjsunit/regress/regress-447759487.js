// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-object-tracking

function __f_2() {
  var __v_13 = {};
  var __v_14 = {
    y: 2.5,
    x: /* NumberMutator: Replaced 0 with 6 */6
  };
  __v_13 = [];
  for (var __v_15 = 0; __v_15 < 2; ++__v_15) {
    __v_12 = __v_13.x;
    __v_13 = __v_14;
  }
}
%PrepareFunctionForOptimization(__f_2);
__f_2();
__f_2();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
