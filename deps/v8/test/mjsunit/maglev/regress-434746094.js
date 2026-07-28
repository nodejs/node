// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let v0 = [10,9,-1791955665,10000,-4294967297,-4294967295,-40480,-1608];
function f1() {
    try {
        v0.slice();
        v0++;
    } catch(e4) {
    }
    return v0;
}
%PrepareFunctionForOptimization(f1);
f1();
f1();
%OptimizeFunctionOnNextCall(f1);
f1();
