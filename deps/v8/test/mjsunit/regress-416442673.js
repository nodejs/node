// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation

let v0 = [-191147.64555024123,1.0];
function f1() {
    const v2 = [255,4,2,-1731910033,-4294967297,16283];
    let v3 = v2[3];
    v3--;
    v2[5] = v2;
    v0 = v3;
    return f1;
}
%PrepareFunctionForOptimization(f1);
const t9 = f1();
t9();
%OptimizeFunctionOnNextCall(f1);
f1();
