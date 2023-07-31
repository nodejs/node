// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
    for (let v2 = 0; v2 < 5; v2++) {
        let v4 = BigInt(v2);
        BigInt.asUintN(64, v4++ >> 4294967297n);
    }
}

%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
