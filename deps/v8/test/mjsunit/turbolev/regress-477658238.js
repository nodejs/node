// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --fuzzing --jit-fuzzing
// Flags: --max-inlined-bytecode-size-small=0 --single-threaded

for (let __v_12 = 0; __v_12 < 5; __v_12++) {
  const __v_14 = { c0: 9007199254740990 };
  let __v_15 = 0;
  const __v_16 = 2 ** 32 + (__v_14.c0 & 1);
  (2 >>> __v_15) - __v_16;
  const __v_17 = %OptimizeOsr();
  function __f_2() { return __v_12; }
}
