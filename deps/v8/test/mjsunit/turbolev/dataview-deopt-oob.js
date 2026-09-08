// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

let big_index = 0x80000000;
let ab = new ArrayBuffer(big_index + 0x40);
let dv = new DataView(ab);

function test_read(dv, index) {
  index |= 0;
  return dv.getInt8(index);
}

%PrepareFunctionForOptimization(test_read);
test_read(dv, 0x40000000);
%OptimizeFunctionOnNextCall(test_read);
test_read(dv, 0x40000000);
assertOptimized(test_read);

// Under 64-bit, offset -2147483648 deopts and throws RangeError.
assertThrows(() => test_read(dv, -2147483648), RangeError);
assertUnoptimized(test_read);

function test_write(dv, index, val) {
  index |= 0;
  dv.setInt8(index, val);
}

%PrepareFunctionForOptimization(test_write);
test_write(dv, 0x40000000, 1);
%OptimizeFunctionOnNextCall(test_write);
test_write(dv, 0x40000000, 2);
assertOptimized(test_write);

// Under 64-bit, offset -2147483648 deopts and throws RangeError.
assertThrows(() => test_write(dv, -2147483648, 99), RangeError);
assertUnoptimized(test_write);

function test_read32(dv, index) {
  index |= 0;
  return dv.getInt32(index);
}

%PrepareFunctionForOptimization(test_read32);
test_read32(dv, 0x40000000);
%OptimizeFunctionOnNextCall(test_read32);
test_read32(dv, 0x40000000);
assertOptimized(test_read32);

assertThrows(() => test_read32(dv, -2147483648), RangeError);
assertUnoptimized(test_read32);

function test_write32(dv, index, val) {
  index |= 0;
  dv.setInt32(index, val);
}

%PrepareFunctionForOptimization(test_write32);
test_write32(dv, 0x40000000, 1);
%OptimizeFunctionOnNextCall(test_write32);
test_write32(dv, 0x40000000, 2);
assertOptimized(test_write32);

assertThrows(() => test_write32(dv, -2147483648, 99), RangeError);
assertUnoptimized(test_write32);
