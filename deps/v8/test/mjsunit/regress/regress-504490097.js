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

class Empty {}
function addD(o, v) { o.d = v; }
%PrepareFunctionForOptimization(addD);
for (let i = 0; i < 500; i++) addD(new Empty(), 3.14);
%OptimizeMaglevOnNextCall(addD);

const attack = new Empty();
addD(attack, holeNaN);

function foo() {
  return attack.d;
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
let v = foo();
assertTrue(isNaN(v));

attack.d = 42;

assertEquals(42, attack.d);
assertEquals(42, foo());
