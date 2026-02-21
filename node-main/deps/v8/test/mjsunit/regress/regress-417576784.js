// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function foo() {
    var a = [];
    a[10012] = 1;
    var r = [undefined].concat(a);
    return r[0];
}

%PrepareFunctionForOptimization(foo);
assertEquals(undefined, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(undefined, foo());
