// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert-types

const arr = [1.1,,];

function foo(b) {
    let x = arr[1];
    const phi = b ? x : 3.35;
    phi >>> phi;
    const v10 = !phi;
}

%PrepareFunctionForOptimization(foo);
foo(false);
%OptimizeMaglevOnNextCall(foo);
foo(true);
