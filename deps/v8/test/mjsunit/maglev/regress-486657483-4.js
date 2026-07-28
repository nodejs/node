// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function make_heapnum(v) { const a=[1.1]; a[0]=v; return a[0]; }

const H0 = make_heapnum(24440.0);
const H1 = make_heapnum(-1.0);

function foo(c, o) {
  let x = c ? H1 : H0;
  x + 2;
  o.x = x;
}

let o = { x : "abc" };
o.x = []; // making non-const and removing field map.

%PrepareFunctionForOptimization(foo);
foo(true, o);

%OptimizeMaglevOnNextCall(foo);
foo(true, o);
