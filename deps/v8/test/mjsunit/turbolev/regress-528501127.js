// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function f(a, b) {
  let tr = (a.w * 1.5) | 0;
  a.x = 1.1;
  let t1 = b.x;
  a.y = 2.2;
  let t2 = b.x;
  b.z = 3.3;
  return tr + t1 + t2;
}
%PrepareFunctionForOptimization(f);
{ let a = {w: 0.5}; let b = {w: 0.5}; b.x = 1.1; f(a, b); }
%OptimizeFunctionOnNextCall(f);
{ let a = {w: 0.5}; let b = {w: 0.5}; b.x = 1.1; f(a, b); }

let o = {w: 0.5};
f(o, o);
assertEquals("w,x,y,z", Object.keys(o).join(","));
assertEquals(2.2, o.y);
assertEquals(3.3, o.z);
