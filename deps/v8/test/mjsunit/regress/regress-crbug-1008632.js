// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

var __v_9690 = function () {};
try {
    (function () {
        __f_1653();
    })()
} catch (__v_9763) {
}
function __f_1653(__v_9774, __v_9775) {
  try {
  } catch (e) {}
    __v_9774[__v_9775 + 4] = 2;
}
(function () {
    %PrepareFunctionForOptimization(__f_1653);
    __f_1653(__v_9690, true);
    %OptimizeFunctionOnNextCall(__f_1653);
    assertThrows(() => __f_1653(), TypeError);
})();
