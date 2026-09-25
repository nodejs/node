// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function* __f_0(__v_1) {
  for (let __v_2 = 0; __v_2 < __v_1; __v_2++) {
    for (let __v_3 = 0; __v_3 < __v_1; __v_3++) {
      Math.acos(false);
      yield __v_2 * 10 + __v_3;
    }
  }
}
%PrepareFunctionForOptimization(__f_0);
let __v_0 = __f_0(4);
__v_0.next().value;
%OptimizeFunctionOnNextCall(__f_0);
__v_0 = __f_0();
