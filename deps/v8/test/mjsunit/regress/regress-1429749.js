// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
    for (let v0 = 0; v0 < 5; v0++) {
        for (let i2 = Uint8ClampedArray; i2 >= Uint8ClampedArray;) {
            i2 = +i2 - Uint8ClampedArray;
        }
    }
}

%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
test();
