// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax
const v1 = new Map();

function f2() {
    v1.size |= 1466662225;
    return v1;
}

%PrepareFunctionForOptimization(f2);
f2();
%OptimizeFunctionOnNextCall(f2);
f2();
