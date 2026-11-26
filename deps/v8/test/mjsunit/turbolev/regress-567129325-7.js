// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let o = { x: 42 };

function foo(x, y) {
  let d = x + 0.5;
  let c = (y & 1) < 2;
  let p = x;
  let acc = 0.5;
  for (let i = 0; i < 1; i++) {
    acc += p;
    if (!c) p = d;
  }
  let q = c ? p : "a";
  let z = c ? 0 : 1;
  let r = acc + (q + z);
  o.x = x;
  return r;
}

%PrepareFunctionForOptimization(foo);
foo(0, 0);

%OptimizeFunctionOnNextCall(foo);
foo(%AllocateHeapNumber(), 0);
%HeapObjectVerify(o);
