// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const A = Object.prototype;
A.has = 'TRUE';
A[0] = 7;
for (let i = 0; i < 129; i++) A['f' + i] = i;
const big = {};
Object.defineProperty(big, 'g0', {enumerable: true, get() { throw 1; }});
for (let i = 0; i < 1024; i++) big['p' + i] = 0;
try { Object.assign(A, big); } catch (e) {}
delete A.f128;

function h(A, trigger) {
  A[0];
  if (trigger) {
    var o = {
      a: 1.1, b: 1.1, c: 1.1, d: 1.1, e: 1.1,
      f: -1.1587936715964575e+148,
      g: 1.1, h: 1.1, i: 1.1, j: 1.1, k: 1.1, l: 1.1, m: 1.1,
    };
  }
  let v = A.has;
  return v.length;
}

%PrepareFunctionForOptimization(h);
h(A, false);
h(A, false);
%OptimizeFunctionOnNextCall(h);
h(A, true);
