// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --fuzzing --jit-fuzzing

function foo() {
    const obj = {
        a: class A {
        },
        constructor() {
            super.prop;
        },
    };
    return obj;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
