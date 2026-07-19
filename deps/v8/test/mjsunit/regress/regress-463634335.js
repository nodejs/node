// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function __wrapTC(permissive = true) {}
function __f_0() {
  for (let __v_1 = 0; __v_1 < 5; __v_1++) {
    const __v_2 = __wrapTC();
    ({ "g": __v_0 } = "c");
    Math.atanh(__v_0);
  }
}
%PrepareFunctionForOptimization(__f_0);
%PrepareFunctionForOptimization(__wrapTC);
__f_0();
%OptimizeMaglevOnNextCall(__f_0);
__f_0();
