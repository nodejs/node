// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --no-turbofan

async function __f_0() {
  try {
    return __f_15;
    function __f_2() {
      return __f_2;
    }
  } catch (__v_3) {
      await 0;
  }
}
%PrepareFunctionForOptimization(__f_0);
try {
  __f_0();
} catch (e) {}
%OptimizeMaglevOnNextCall(__f_0);
try {
  __f_0();
} catch (e) {}
