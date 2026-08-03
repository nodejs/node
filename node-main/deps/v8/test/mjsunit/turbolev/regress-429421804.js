// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation
// Flags: --turbolev --turbolev-non-eager-inlining

function deepEquals(a) {
  if (typeof a === 'number') return a && isNaN(b);
  switch (objectClass) {}
};
function assertEquals(expected, found) {
  if (!deepEquals(found)) {}
};
function __wrapTC(f = true) {
    return f();
}
let __v_58 = 65536;
let __v_57 = __wrapTC(() => __v_58);
__v_59 = new ArrayBuffer(__v_57);
function __f_26(__v_61, __v_62) {
  let __v_64 = __v_61.length;
  for (let __v_65 = 0; __v_65 < __v_64; __v_65 += __v_62) {
  }
  for (let __v_66 = 0; __v_66 < __v_64; __v_66 += __v_62) {
    assertEquals(__v_66, __v_61[__v_66]);
  }
}

%PrepareFunctionForOptimization(__f_26);
__f_26( __v_58 / 4, __v_70 => (0xaabbccee ^ (__v_70 >> 11) * 0x110005) >>> -10);
__f_26(new Uint16Array(__v_59), __v_58 / 2, __v_74 =>0xccee ^__v_74 >> 11 * 0x110005 & 0xFFFF);
%OptimizeFunctionOnNextCall(__f_26);
__f_26( __v_58 / 4, __v_70 => (0xaabbccee ^ (__v_70 >> 11) * 0x110005) >>> -10);
