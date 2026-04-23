// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax

function createArray(x) { return new Array(x, 1.1); }

// Construct the hole-pattern float64:
const dv = new DataView(new ArrayBuffer(8));
dv.setUint32(0, 0xFFF7FFFF, true);
dv.setUint32(4, 0xFFF7FFFF, true);
const x = dv.getFloat64(0, true);

%PrepareFunctionForOptimization(createArray);
let xs = createArray(x);
assertArrayEquals([NaN, 1.1], xs);

%OptimizeFunctionOnNextCall(createArray);
xs = createArray(x);
assertArrayEquals([NaN, 1.1], xs);
