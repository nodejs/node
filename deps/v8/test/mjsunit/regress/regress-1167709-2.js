// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

function __f_0() {
}
function __f_3( __v_7, ...__v_8) {
  return new __f_0( ...__v_8);
}
function __f_5() {
  __f_3();
}
%PrepareFunctionForOptimization(__f_5);
__f_5();
%OptimizeFunctionOnNextCall(__f_5);
__f_5();
