// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_6() {
  var __v_7 = [0];
  Object.preventExtensions(__v_7);
  for (var __v_6 = -2; __v_6 < 19; __v_6++) __v_7.shift();
  __f_7(__v_7);
};
%PrepareFunctionForOptimization(__f_6);
__f_6();
__f_6();
%OptimizeFunctionOnNextCall(__f_6);
__f_6();
function __f_7(__v_7) {
  __v_7.pop();
}
