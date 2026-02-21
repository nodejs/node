// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var __v_0 = 5;
function __f_1() {
  for (var __v_6 = 0; __v_6 < __v_0; __v_6++) {
    if (__v_6 % __v_6 == 0) {
      "a".split();
    }
  }
}

%PrepareFunctionForOptimization(__f_1);
__f_1();
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
