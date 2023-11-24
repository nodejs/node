// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
    const v11 = 8 * (8 >> undefined);
    const v12 = v11 + v11;
    return v12 + v12;
}

%PrepareFunctionForOptimization(foo);
assertEquals(256, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(256, foo());
