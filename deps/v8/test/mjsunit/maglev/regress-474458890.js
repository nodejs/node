// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_2() {
    var __v_1 = {
      get: function () {
        __v_0[__getRandomProperty()] = __v_1;
      }
    };
}
%PrepareFunctionForOptimization(__f_2);
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
