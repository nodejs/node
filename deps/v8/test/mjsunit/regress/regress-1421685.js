// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbo-load-elimination
// Flags: --no-turbo-loop-variable

function test() {
  for (let v1 = 0; v1 < 1; v1++) {
    const v3 = BigInt(v1);
    ([("1244138209").length]).includes(5, -2147483649);
    v3 << 51n;
  }
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
