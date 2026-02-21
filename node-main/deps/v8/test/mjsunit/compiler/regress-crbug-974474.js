// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
    const y = x == 42;
    () => {y};
    if (y) { Object(); }
    [!!y];
    return y;
}

%PrepareFunctionForOptimization(foo);
foo(42); foo(42);
%OptimizeFunctionOnNextCall(foo);
foo(42);
