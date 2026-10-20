// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function* opt_me(flag, a1) {
    if(flag)
        %OptimizeOsr();
    for (let j = 0; j < 1; j++) {
    }
    for (let k = 0; k < 1; k++) {
        const tmp = a1 || 1;
        use = tmp % 4;
        yield 1;
    }
}
%PrepareFunctionForOptimization(opt_me);
opt_me().next();
opt_me( 123).next();
