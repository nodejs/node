// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_6() {
    for (var __v_32 = 0; __v_32 < 3; ++__v_32) {
      if (__v_32 == 1) {
        %OptimizeOsr();
      }
      [ __v_12][0]?.constructor;
    }
}
%PrepareFunctionForOptimization(__f_6);

assertDoesNotThrow(() => __f_6());
var __v_12 = '';
