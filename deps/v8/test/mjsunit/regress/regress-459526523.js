// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function __f_12() {
  const __v_38 = [];
  __v_38[8] = 4.2;
  for (let __v_39 = 0; __v_39 < 5; __v_39++) {
    let __v_40 = __v_38[4];
    for (let __v_41 = 0; __v_41 < 5; __v_41++) {
      __v_39 = __v_40;
      try {
        __v_40.n();
      } catch (__v_42) {}
      ++__v_40;
    }
  }
}
  %PrepareFunctionForOptimization(__f_12);
  __f_12();
  %OptimizeFunctionOnNextCall(__f_12);
  __f_12();
