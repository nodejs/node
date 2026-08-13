// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var data = new Uint8ClampedArray(16);
for (var i = 0; i < data.length; i++) data[i] = i + 100;

function f(idx, obj) {
  var v = data[idx];  // idx can be negative -> undefined (handle-OOB).
  data[0] = v;        // Clamped store of the byte-or-undefined phi.
  var r = obj.x;      // Eager deopt here when obj's map changes.
  return [v, r];
}

var stable = {x: 42};

%PrepareFunctionForOptimization(f);
// Warm up with both in-bounds and out-of-bounds indices so the keyed load
// feedback handles OOB without deopting.
assertEquals([103, 42], f(3, stable));
assertEquals(103, data[0]);
assertEquals([undefined, 42], f(-1, stable));
assertEquals(0, data[0]);
%OptimizeFunctionOnNextCall(f);

// Optimized: byte path.
assertEquals([105, 42], f(5, stable));
assertEquals(105, data[0]);
// Optimized: undefined path clamps to 0.
assertEquals([undefined, 42], f(-1, stable));
assertEquals(0, data[0]);

// Trigger a deopt while the phi is live: v must materialize as 'undefined',
// not as NaN, 0, or the undefined NaN pattern boxed as a number.
var other = {x: 43, y: 1};
var [v, r] = f(-1, other);
assertEquals(undefined, v);
assertEquals(43, r);
assertEquals(0, data[0]);

// And the byte path across the same deopt shape.
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
f(5, other);
var other2 = {x: 44, y: 1, z: 2};
assertEquals([107, 44], f(7, other2));
assertEquals(107, data[0]);
