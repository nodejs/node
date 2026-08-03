// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


var __v_6 = "gaga";
function __f_3() {
  let __v_8 = Math.min(Infinity ? __v_6 : 0) / 0;
  return __v_8 ? 1 : 0;
}
%PrepareFunctionForOptimization(__f_3);
__f_3();
%OptimizeFunctionOnNextCall(__f_3);
__f_3();
