// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Regression test for a miscompile of a Word32And followed by a comparison
// against zero. A 32-bit value is not guaranteed to be sign-extended in its
// 64-bit register: a zero-extending Uint32 load followed by a Word32Xor with -1
// can leave the upper 32 bits set. If the zero test is performed on the full
// register width, those dirty upper bits make a zero i32 result look non-zero
// and invert the branch. The 32-bit zero test must normalize the result first.

var arr = new Uint32Array([0x55555555, 0xAAAAAAAA]);

function andZero(i, j) {
  var a = arr[i] ^ -1;
  var b = arr[j] ^ -1;
  // (~a & ~b) === 0, since ~a & ~b == ~(a | b) == ~0xFFFFFFFF == 0 (i32).
  return (a & b) === 0;
}

%PrepareFunctionForOptimization(andZero);
for (let k = 0; k < 200000; k++) {
  arr[0] = 0xFFFFFFFF;
  arr[1] = 0x00000000;
  andZero(0, 1);
}
arr[0] = 0x55555555;
arr[1] = 0xAAAAAAAA;
%OptimizeFunctionOnNextCall(andZero);
assertEquals(true, andZero(0, 1));
