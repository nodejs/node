// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --maglev --maglev-assert-types --maglev-non-eager-inlining

function __wrapTC(f, permissive = true) {
  try { return f(); } catch (e) {}
}

function __f_1() {
  for (let __v_7 = 0; __v_7 < 5; __v_7++) {
    const __v_8 = __wrapTC(() => [0.596903954456784,,][1]);
    const __v_9 = __wrapTC(() => __v_8 % -65536);
    Math.atanh(__v_8);
    if (__v_8 != null && typeof __v_8 == "object") Object.__getRandomProperty(), {};
    %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(__f_1);
__f_1();
__f_1();
%OptimizeFunctionOnNextCall(__f_1);
__f_1();
