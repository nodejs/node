// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function make_heapnum(v) { const a=[1.1]; a[0]=v; return a[0]; }

const H0 = make_heapnum(24440.0);
const H1 = make_heapnum(-1.0);

function foo() {
  let x = H0;
  for (let i = 0; i < 1; ++i) {
    if (i) x = H1;
    x + 0.25;
  }
  let c = !!x;

  return c;
}

%PrepareFunctionForOptimization(foo);
assertEquals(true, foo());

%OptimizeMaglevOnNextCall(foo);
assertEquals(true, foo());
