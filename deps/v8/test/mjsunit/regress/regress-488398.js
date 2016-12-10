// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var __v_10 = 4294967295;
__v_0 = [];
__v_0.__proto__ = [];
__v_16 = __v_0;
function __f_17(__v_16, base) {
  __v_16[base + 1] = 1;
  __v_16[base + 4] = base + 4;
}
__f_17(__v_16, true);
__f_17(__v_16, 14);
%OptimizeFunctionOnNextCall(__f_17);
__f_17(__v_16, 2048);
