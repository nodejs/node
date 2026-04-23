// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --verify-heap

function make_heapnum(v) { const a=[1.1]; a[0]=v; return a[0]; }

const H0 = make_heapnum(-1.0);
const H1 = make_heapnum(-1.0);

function f(c, target) {
  let v = c ? H0 : H1;
  let y = v + 0.1;
  target.p = v;

  return y;
}

%PrepareFunctionForOptimization(f);

let target = {p:{x:1}};
f(0, target);

%OptimizeMaglevOnNextCall(f);
gc();

(f(1, target));
