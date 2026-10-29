// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --stress-gc-during-compilation

const __v_0 = class __c_0 extends Array {
  constructor() {
    super();
    this.y = 1;
  }
};
function __f_1() {
  var __v_2 = new __v_0();
}
  %PrepareFunctionForOptimization(__f_1);
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
