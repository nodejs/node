// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

let __v_101 = 0;
function __f_37() {
  async function __f_38() {
      for (let __v_108 = 0; __v_108 < 5; __v_108++) {
        for (let __v_109 = 0; (() => {
          if (__v_108 != null && typeof __v_108 == "object") {
            Object.defineProperty(__v_109, {});
          }
        })();) {
          await 7;
        }
      }
  }
  __f_38();
  if (__v_101 < 4) {
    const __v_111 = () => __v_101 + 1;
    __f_36(__v_101 = __v_111);
  }
}
__f_36 = __f_37;
%PrepareFunctionForOptimization(__f_37);
__f_37();
%OptimizeFunctionOnNextCall(__f_37);
__f_37();
