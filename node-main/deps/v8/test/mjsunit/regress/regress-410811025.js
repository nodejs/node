// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


(function () {
  async function __f_13() {
      for (let __v_24 = 0; __v_24 < 4; __v_24++) {
        __v_24.__lookupGetter__();
        for (let __v_26 = 0; __v_26 == 6; __v_26++) {
          await 7;
        }
      }
  }
  %PrepareFunctionForOptimization(__f_13);
  __f_13();
  %OptimizeFunctionOnNextCall(__f_13);
  __f_13();
})();
