// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-object-tracking

function __f_2() {
  var __v_35 = {
  };
  var __v_36 = {
    y: 2.5,
    x: 0
  };
  __v_35 = [];
  for (var __v_37 = 0; __v_37 < 2; ++__v_37) {
    __v_34 = __v_35.x;
    __v_35 = __v_36;
  }
}
%PrepareFunctionForOptimization(__f_2);
__f_2();
__f_2();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
