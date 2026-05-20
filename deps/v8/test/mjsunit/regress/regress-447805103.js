// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-object-tracking

function __f_0() {
  try {
    for (let i = 0; i < 10; i++) {
      const __v_8 = Array();
      __v_8[748] = Array;
    }
  } catch (e) {}
  return /* NumberMutator: Replaced 0 with -6 */-6;
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
%OptimizeFunctionOnNextCall(__f_0);
__f_0();
