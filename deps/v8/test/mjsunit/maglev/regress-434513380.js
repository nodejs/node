// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function f1() {
    v0 = Math.sqrt(0);
    f2(v0);
}

function f2() {}

function f5(v0) {
    %OptimizeMaglevOnNextCall(f1);
    f1();
    gc();
}

let v0 = 1;
%PrepareFunctionForOptimization(f1);
f5();
f5();
