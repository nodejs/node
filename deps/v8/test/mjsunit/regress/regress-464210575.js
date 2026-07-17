// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const __v_85 = [function() {}];
function __f_17() {
  const __v_98 = __v_85.at(__v_85);
  return __v_85.forEach;
}
%PrepareFunctionForOptimization(__f_17);
__f_17();
__f_17();
%OptimizeMaglevOnNextCall(__f_17);
__f_17();
