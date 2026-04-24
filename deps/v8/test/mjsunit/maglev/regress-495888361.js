// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let __callGC;
function __wrapTC(f = true) {
    return f();
}
const __v_103 = /$/;
function __f_23() {
    try {
      for (let __v_106 = 0, __v_107 = 10; (() => {
        __v_107--;
        return __v_108 = __v_107;
      })();) {}
      v1[Symbol]();
    } catch (__v_109) {
    }
    return __v_103;
}
const __v_104 = __wrapTC(() => %PrepareFunctionForOptimization(__f_23));
  __f_23();
const __v_105 = __wrapTC(() => %OptimizeFunctionOnNextCall(__f_23));
  __f_23();
