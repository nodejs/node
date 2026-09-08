// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Same as the addition case, but through the multiplication identity.
const f64 = new Float64Array(1);
const u32 = new Uint32Array(f64.buffer);
u32[0] = 0xFFF7FFFF;
u32[1] = 0xFFF7FFFF;

function test(packed_arr, ta) {
  let inlined_arr = [1.0, 1.1];
  let res = ta[0] * inlined_arr[0];
  packed_arr[0] = res;
}

%PrepareFunctionForOptimization(test);
test([1.1, 2.2], f64);
%OptimizeFunctionOnNextCall(test);

const packed = [10.1, 20.2];
test(packed, f64);

assertTrue(0 in packed);
assertEquals(NaN, packed[0]);
assertFalse(packed.includes(undefined));
assertEquals('[null,20.2]', JSON.stringify(packed));
