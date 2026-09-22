// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-lazy-feedback-allocation
// Flags: --turbolev-escape-analysis

function Obj() {
  this.x = 1.5;
}

var arr = [];

function f() {
  let a = new Obj();
  let b = new Obj();
  let c = new Obj();
  for (let i = 0; i < 42; i++) {
    b.x = c.x;
    arr[1048182] = {};
    a.x = b.x;
  }
  return a.x;
}

%PrepareFunctionForOptimization(f);
assertEquals(1.5, f());
%OptimizeFunctionOnNextCall(f);
assertEquals(1.5, f());
