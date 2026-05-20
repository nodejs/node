// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function __wrapTC(f) {
    return f();
}
function __f_0() {
  try {
    const __v_4 = Array();
      __v_4.reduceRight(__f_0);
  } catch (e) {}
}
const __v_0 = __wrapTC(() => __f_0());
%PrepareFunctionForOptimization(__f_0);
const __v_1 = __wrapTC(() => new __f_0);
%OptimizeFunctionOnNextCall(__f_0);
__f_0();
