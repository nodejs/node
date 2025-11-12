// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

let ab = new ArrayBuffer(4);
let dv = new DataView(ab);

const kA = 0x01_80;
const kB = 0xFF_FF_FF_7F | 0;  // sign-extended 0xFF_7F

// Explicitly write the big-endian int16 representations
// of these values into the buffer:
dv.setInt8(0, 0x01, false);
dv.setInt8(1, 0x80, false);
dv.setInt8(2, 0xFF, false);
dv.setInt8(3, 0x7F, false);

function f(dv, index) {
  return dv.getInt16(index, false);  // big-endian read.
}

%PrepareFunctionForOptimization(f);
assertEquals(kA, f(dv, 0));
assertEquals(kB, f(dv, 2));
%OptimizeMaglevOnNextCall(f);
assertEquals(kA, f(dv, 0));
assertEquals(kB, f(dv, 2));
