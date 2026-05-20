// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

class C extends Array { }

function foo(a) {
    a[0];
    return a;
}

%PrepareFunctionForOptimization(foo);
foo([,]);
foo(new C);

%OptimizeFunctionOnNextCall(foo);
foo([,]);
