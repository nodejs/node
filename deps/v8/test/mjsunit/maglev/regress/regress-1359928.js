// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --no-concurrent-recompilation

function __f_0() {
  var __v_1;
  try {
    class __c_0 extends (__v_4) {}
  } catch {
    console.log("soozie");
  }
  try {
    Object.defineProperty(__v_2, 'x');
  } catch {}
  try {
    console.log("foozie");
    class __c_2 extends (eval('delete obj.x'), class {}) {}
  } catch (__v_7) {
    console.log("boozie");
    __v_1 = __v_7;
  }
}
%PrepareFunctionForOptimization(__f_0);
__f_0();
%OptimizeMaglevOnNextCall(__f_0);
__f_0();
