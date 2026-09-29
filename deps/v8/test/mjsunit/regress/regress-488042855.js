// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v1 = Symbol.iterator;

function f4(a5) {
    const v8 = a5 + 16;
    try { v8.constructor(v1); } catch (e) {}
    return f4;
}
%PrepareFunctionForOptimization(f4);
f4();
f4();
%OptimizeFunctionOnNextCall(f4);
f4();
f4();
