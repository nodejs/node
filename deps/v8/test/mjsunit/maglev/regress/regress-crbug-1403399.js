// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

function __f_0() {
    for (let __v_3 = 0; __v_3 < 52; ++__v_3) {
      let __v_4 = __v_3 | 0;
      switch (__v_4) {
        case 28:
            if (__v_3 != null && typeof __v_3 == "object") {
              try {
                 Object.defineProperty(  {
                   get: function () {
                           ({get: function () {
                             return __v_4;
                           }})
                   }
                 });
              } catch (e) {}
            }
        case 29:
        case 31:
        case 32:
        case 33:
            __v_4 += 1;

        case 34:
      }
    }
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
%OptimizeMaglevOnNextCall(__f_0);
__f_0();
