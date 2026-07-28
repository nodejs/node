// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

const m = Math.cbrt();
function f() {
    const arr = [];
    arr[1] = 4.2;
    const k = arr[0] ?? m;
    k ^ k;
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeMaglevOnNextCall(f);
f();
