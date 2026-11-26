// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev

// Math.max(x, x) and Math.min(x, x) reduced by the graph optimizer, with x a
// tagged Smi. The graph builder must not reduce the calls (speculation is
// disabled and x is the result of a call that is only inlined later).

// Large enough to not be inlined eagerly.
function getX(o) {
  let s = 0;
  s += o.y; s += o.y; s += o.y; s += o.y; s += o.y; s += o.y;
  s += o.y; s += o.y; s += o.y; s += o.y; s += o.y; s += o.y;
  return o.x;
}

function maxSame(o) {
  const x = getX(o);
  return Math.max(x, x) + x;
}

function minSame(o) {
  const x = getX(o);
  return Math.min(x, x) + x;
}

function max3(o) {
  const x = getX(o);
  return Math.max(x, x, x) + x;
}

function NonNumber(x) {
  this.y = 0;
  this.x = x;
}
new NonNumber('a');
new NonNumber({});

function Smi(x) {
  this.y = 0;
  this.x = x;
}

for (const f of [maxSame, minSame, max3]) {
  // Deopt in the Math.max/min call to disable speculation there.
  %ClearFunctionFeedback(getX);
  %PrepareFunctionForOptimization(getX);
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 10; i++) f(new NonNumber('1'));
  %OptimizeFunctionOnNextCall(f);
  f(new NonNumber('1'));
  f(new NonNumber({}));

  %ClearFunctionFeedback(getX);
  %PrepareFunctionForOptimization(getX);
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 10; i++) getX(new Smi(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(10, f(new Smi(5)));
  assertEquals(-6, f(new Smi(-3)));
}
