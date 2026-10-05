// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const reg = new FinalizationRegistry(() => {});
const keep = {};
let sink = null;

function mk(i) {
  const o = {};
  o.a0 = i;
  o.a1 = i;
  o.a2 = i;
  o.a3 = i;
  o.a4 = i;
  o.a5 = i;
  o.a6 = i;
  o.a7 = i;
  return o;
}

function f(i) {
  const o = mk(i);
  reg.register(keep, i, o);
  const x = o.a5;
  const arr = new Array(4000);
  const y = o.a6;
  sink = arr;
  return x + y;
}

%PrepareFunctionForOptimization(mk);
%PrepareFunctionForOptimization(f);
for (let i = 0; i < 10; i++) {
  f(i);
}
%OptimizeFunctionOnNextCall(f);
let s = 0;
for (let i = 0; i < 2000; i++) {
  s += f(i);
}
