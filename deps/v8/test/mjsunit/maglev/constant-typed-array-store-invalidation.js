// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Stores into a typed array backed by a compile-time-constant reference
// (e.g. an asm.js/wasm-style heap view) must not be treated as an
// arbitrary side effect: they only write one element and must not
// invalidate unrelated cached maps, loaded properties or object shapes.

const buffer = new ArrayBuffer(64);
const heap16 = new Uint16Array(buffer);
const heap32 = new Int32Array(buffer);

function obj() {
  return {a: 1, b: 2};
}

function loop(n) {
  const o = obj();
  let sum = 0;
  for (let i = 0; i < n; i++) {
    // A store through a module-scoped typed array reference, which the
    // optimizer treats as accessing a constant view.
    heap16[i & 31] = (i * 7) & 0xffff;
    // A load on an unrelated object whose map/shape should remain known
    // across the store above.
    sum += o.a + o.b;
  }
  return sum;
}
%PrepareFunctionForOptimization(loop);
const expected = loop(40);
%OptimizeFunctionOnNextCall(loop);
assertEquals(expected, loop(40));

// Values actually landed correctly (not just "didn't crash").
assertEquals((39 * 7) & 0xffff, heap16[39 & 31]);

function loop32(n) {
  let sum = 0;
  for (let i = 0; i < n; i++) {
    heap32[i & 15] = i - 1000;
    sum += heap32[i & 15];
  }
  return sum;
}
%PrepareFunctionForOptimization(loop32);
const expected32 = loop32(50);
%OptimizeFunctionOnNextCall(loop32);
assertEquals(expected32, loop32(50));
