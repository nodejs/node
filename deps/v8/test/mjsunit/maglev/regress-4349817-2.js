// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --expose-gc --verify-heap

function f(arr) {
    let phi = arr ? 1 : 42.5;
    phi |= 0;
    arr[5] = phi;
}

let arr = Array(10);
gc();
gc();

%PrepareFunctionForOptimization(f);
f(arr);
%OptimizeMaglevOnNextCall(f);
f(arr);
gc();
