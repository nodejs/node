// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --no-turboshaft-loop-unrolling

function f(o1, o2, n) {
  if (n) {
    let w = o1;
    w.p;
  } else {
    let w = o1;
    w.p;
  }

  let x = o1.p;
  let b = o2;
  let total = 0;
  for (let i = 0; i < 10; i++) {
    b.p = i;
    total += o1.p;
    if (i === 2) {
      o1.q = 5;
      b = o1;
    }
  }
  return total + x;
}

%PrepareFunctionForOptimization(f);
f({p: 7}, {p: 7, r: 1}, 0);
f({p: 7}, {p: 7, r: 1}, 1);
%OptimizeFunctionOnNextCall(f);

let res = f({p: 7}, {p: 7, r: 1}, 0);
assertEquals(70, res);
assertOptimized(f);
