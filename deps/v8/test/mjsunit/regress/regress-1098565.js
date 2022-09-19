// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
    return arguments;
}

arr = [];
arr.length=0x8000;
g = f.bind(null,...arr);

function test() {
    return g();
}

%PrepareFunctionForOptimization(f);
%PrepareFunctionForOptimization(test);
test();
%OptimizeFunctionOnNextCall(test);
assertEquals(test().length, arr.length);
