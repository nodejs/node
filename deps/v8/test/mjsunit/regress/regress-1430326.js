// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
    for (var x in this) {};
    if (true) x++;
    return x++;
};

%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
