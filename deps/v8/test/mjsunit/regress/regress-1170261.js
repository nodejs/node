// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo(unused1, unused2, bigint) {
    const temp = -bigint;
}

function bar() {
    const arr = Array();
    const obj = Object();
    arr.reduce(foo, 0)
}

%PrepareFunctionForOptimization(foo);
foo(0, 0, 2316465375n);
%OptimizeFunctionOnNextCall(foo);
foo(0, 0, 2316465375n);

%PrepareFunctionForOptimization(bar);
bar();
%OptimizeFunctionOnNextCall(bar);
bar();
