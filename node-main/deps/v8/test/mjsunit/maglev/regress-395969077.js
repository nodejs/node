// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var __v_5 = new String();
function __f_1() {
  'stringcontent' + __v_5;
  'stringcontent' + __v_5;
}
%PrepareFunctionForOptimization(__f_1);
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
