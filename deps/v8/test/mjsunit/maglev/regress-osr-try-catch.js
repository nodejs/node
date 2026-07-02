// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

let __callGC;
function __f_0() {
    L4: for (let __v_2 = 0; __v_2 < 5; __v_2++) {
      const __v_3 = __v_2++;
      try {
        __v_0.__v_3;
        try {
          break L4;
        } catch (__v_4) {}
      } catch (__v_5) {}
      for (let __v_6 = 0; __v_6 < 5; __v_6++) {
        const __v_7 = %OptimizeOsr();
      }
      eval();
    }
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
