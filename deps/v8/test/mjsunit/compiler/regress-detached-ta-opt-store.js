// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing

const SIZE = 1024 * 1024;
const ab = new ArrayBuffer(SIZE);
const ta = new Uint32Array(ab);
ta.foo = 1;
const ab_detached = new ArrayBuffer();
const ta_detached = new Uint32Array(ab_detached);
ta_detached.foo = 1;
%ArrayBufferDetach(ab_detached);

function opt(ta_param, idx, val) {
  ta_param[idx] = val;
}
%PrepareFunctionForOptimization(opt);
opt(ta_detached, 0);

function test(idx) {
  opt(ta, idx);
}
%PrepareFunctionForOptimization(test);
test(0x1234);
%OptimizeFunctionOnNextCall(test);
test(0x1234);
%ArrayBufferDetach(ab);
test(0);
