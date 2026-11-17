// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turbofan --allow-natives-syntax

const P = Array.prototype;
const Obj = Object;
let s;

function foo(o, t) {
  o[0];
  let r = 0;
  if (t) r = Obj`hello`;
  const v = o.x;
  s = r;
  return v;
}

new WeakMap().set(P, 1);
for (const _ in []) {}
P[0] = 1;
P.x = 1;
Object.defineProperty(P, "x", {value: 1.5, enumerable: false});

%PrepareFunctionForOptimization(foo);
foo(P, false); foo(P, false); foo(P, false);
%OptimizeFunctionOnNextCall(foo);
assertEquals(1.5, foo(P, false));

for (let i = 0; i < 400; i += 3) {
  P["p" + i] = "x";
  P["p" + (i + 1)] = "corrupted";
  P["p" + (i + 2)] = 0;
}

assertEquals(1.5, foo(P, true));
