// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-simplified-lowering

const dv = new DataView(new ArrayBuffer(8));

function bar() {}
%NeverOptimizeFunction(bar);

function foo(dv) {
  const o = {x: {}};
  o.x = -0;
  bar();
  const x = o.x;
  dv.getUint8(x);
  return 1 / (x + -0);
}

%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 5; i++) {
  assertEquals(-Infinity, foo(dv));
}
%OptimizeFunctionOnNextCall(foo);
assertEquals(-Infinity, foo(dv));
