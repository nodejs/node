// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

function v0(v1) {
    v1.apply();
}

function v2() {
    function v3() {
        }
    %PrepareFunctionForOptimization(v0);
    v0(v3);
    %OptimizeFunctionOnNextCall(v0);
    v0(v3);
}

v2();
gc();
assertThrows(function () { v0(2); }, TypeError);
