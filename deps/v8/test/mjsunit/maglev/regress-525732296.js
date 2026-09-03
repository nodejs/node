// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const byte_length = 0xFFFFFFFF;
const buffer = new ArrayBuffer(byte_length);
const dv = new DataView(buffer);

// Write some value at offset 0xFFFFFFFE.
// When we access index -2, it becomes 0xFFFFFFFE as a 32-bit unsigned value.
const u8 = new Uint8Array(buffer);
u8[0xFFFFFFFE] = 0x42;

function read(dv, i) {
  return dv.getInt8(i);
}

assertThrows(() => read(dv, -2), RangeError);

%PrepareFunctionForOptimization(read);
read(dv, 0);
read(dv, 0);
%OptimizeMaglevOnNextCall(read);
read(dv, 0);

assertThrows(() => read(dv, -2), RangeError);
