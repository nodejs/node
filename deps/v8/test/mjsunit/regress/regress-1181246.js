// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

Object.defineProperty(String.prototype, "0", { __v_1: 1});
var __f_2 = function() {
  function __f_2() {
    ''[0];
  };
  %PrepareFunctionForOptimization(__f_2);
  return __f_2;
}();
%PrepareFunctionForOptimization(__f_2);
__f_2();
__f_2();
%OptimizeFunctionOnNextCall(__f_2);
__f_2();
