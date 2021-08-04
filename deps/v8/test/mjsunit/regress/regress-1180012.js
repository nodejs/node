// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

var __v_10 = {};
var __v_9 = [-1];
function __f_7() {
  (__v_10[65535] | 65535) / __v_9[2147483648];
}
%PrepareFunctionForOptimization(__f_7);
__f_7();
__f_7();
%OptimizeFunctionOnNextCall(__f_7);
__f_7();
