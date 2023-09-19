// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

var __v_18 = this;
function __f_3() {
  __v_18.outSideFunc = __f_6;
  function __f_6() {
  }
}
%PrepareFunctionForOptimization(__f_3);
__f_3();
%OptimizeFunctionOnNextCall(__f_3);
__f_3();
