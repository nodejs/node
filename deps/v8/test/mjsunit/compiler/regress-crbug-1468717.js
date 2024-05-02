// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft --turboshaft-load-elimination --allow-natives-syntax

const explore =function() {
    function exploreWithErrorHandling() {
        try {
            explore();
        } catch (e) {
        }
    }
    return exploreWithErrorHandling;
}();
const v7 = new Uint8ClampedArray(7389);
function f8() {
    v7[0] = 0.5;
    const v11 = v7[0];
    explore( v11, []);
}
%PrepareFunctionForOptimization(f8);
f8();
%OptimizeFunctionOnNextCall(f8);
f8();
