// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const ab = new ArrayBuffer(8);
const f64 = new Float64Array(ab);
const u32 = new Uint32Array(ab);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;
const holeNaN = f64[0];

function create(v) { return { key: v }; }

%PrepareFunctionForOptimization(create);
for (let i = 0; i < 100; i++) create(1.5);
%OptimizeMaglevOnNextCall(create);
const o = create(holeNaN);

f64[0] = o.key;

assertTrue(isNaN(o.key));

function test() {
  let v = o.key;
  return v;
}

%PrepareFunctionForOptimization(test);
for (let i = 0; i < 100; i++) test();
%OptimizeFunctionOnNextCall(test);
let result = test();
f64[0] = result;

assertTrue(isNaN(result));

o.key = 42;
assertEquals(42, o.key);
assertEquals(42, test());
