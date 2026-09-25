// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-range-verification
// Flags: --maglev-range-analysis

var __v_0 = 5;
function __f_0() {
  for (var __v_1 = 0; __v_1 < __v_0; __v_1++) {
  }
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
%OptimizeFunctionOnNextCall(__f_0);
__f_0();
