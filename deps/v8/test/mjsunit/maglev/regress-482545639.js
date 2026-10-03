// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-assert-types --single-threaded

function test() {
  let __v_6 = 1;
  let counter = 1;
  for (let __v_7 = __v_6; __v_7 >= __v_6; __v_7 += __v_6) {
    const __v_8 = __v_7++;
    %OptimizeOsr();
    __v_6 = __v_6 && __v_7;
    if (++counter > 1000) return;
  }
}
%PrepareFunctionForOptimization(test);
test();
