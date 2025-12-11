// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
    let str = "aaaaaaaaaaaaaaaaaaaaaaaa";
    for (let i = 0; i < 32768; i++) {
        str += "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    }
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
