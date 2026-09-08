// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// A DataView element store only writes bytes into the view's backing
// buffer; it must not invalidate unrelated cached maps or loaded
// properties, and values must round-trip correctly.

const buffer = new ArrayBuffer(64);
const view = new DataView(buffer);

function obj() {
  return {a: 1, b: 2};
}

function loop(n) {
  const o = obj();
  let sum = 0;
  for (let i = 0; i < n; i++) {
    view.setInt32((i % 4) * 4, i * 3, true);
    view.setFloat64(32, i * 1.5, true);
    sum += o.a + o.b;
  }
  return sum;
}
%PrepareFunctionForOptimization(loop);
const expected = loop(20);
%OptimizeFunctionOnNextCall(loop);
assertEquals(expected, loop(20));

assertEquals(19 * 3, view.getInt32(((19 % 4)) * 4, true));
assertEquals(19 * 1.5, view.getFloat64(32, true));
