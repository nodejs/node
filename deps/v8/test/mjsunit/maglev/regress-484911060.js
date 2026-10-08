// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types

function __f_5() {
  try {
    __v_65 = new Array(-9223372036854775808);
  } catch (__v_72) {}
}
%PrepareFunctionForOptimization(__f_5);
__f_5();
__f_5();
%OptimizeMaglevOnNextCall(__f_5);
__f_5();
