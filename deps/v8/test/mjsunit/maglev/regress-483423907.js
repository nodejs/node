// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const dv = new DataView(new ArrayBuffer(8));
dv.setUint32(0, 0x00000000, true);
dv.setUint32(4, 0xFFF7FFFF, true);
const craftedNaN = dv.getFloat64(0, true);
const holey = [1.1, , 3.3];

function f(c, i) {
  let v;
  if (c) { v = holey[i]; } else { v = craftedNaN; }
  let _ = v | 0;
  return (v ?? 1337) | 0;
}

%PrepareFunctionForOptimization(f);
f(true, 0); f(true, 1); f(false, 0);

%OptimizeFunctionOnNextCall(f);
assertEquals(0, f(false, 0));
