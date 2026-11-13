// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function make_hn0(a) {
  return a - 1.5;
}
%NeverOptimizeFunction(make_hn0);

const hn0 = make_hn0(1.5); // HeapNumber(0.0)
globalThis.g_hn = 1.5;
globalThis.g_hn = 2.5;

const i32_arr = new Int32Array(10);
const smi_holder = { s: 1 };
const smi_target = { y: 1 };

function get_zero(flag) {
  let r = 0;
  if (flag === 101) r = 1;
  if (flag === 102) r = 2;
  if (flag === 103) r = 3;
  if (flag === 104) r = 4;
  if (flag === 105) r = 5;
  return 0;
}

function to_i32(x, warmup, dummy) {
  if (warmup) return 0;
  let z = get_zero(0);
  let elem = i32_arr[x];
  let k = x + z;
  smi_holder.s = k;
  let q = ((elem & 1) < 10) ? x : dummy;
  smi_target.y = q;
  return elem;
}

function foo(c1, warmup, dummy) {
  let p = g_hn;
  let sum = 0.0;
  for (let i = 0; i < 2; i++) {
    sum += p + 0.5;
    if (c1) {
      p = g_hn;
      continue;
    }
    p = 1.5;
  }
  to_i32(p, warmup, dummy);
  return sum;
}

%PrepareFunctionForOptimization(get_zero);
%PrepareFunctionForOptimization(to_i32);
%PrepareFunctionForOptimization(foo);
get_zero(0);
to_i32(1, false, 1);
to_i32(100, false, 1);

foo(false, true, 1);
foo(true, true, 1);

%OptimizeFunctionOnNextCall(foo);
globalThis.g_hn = hn0;
foo(true, false, 1);
%HeapObjectVerify(smi_target);
