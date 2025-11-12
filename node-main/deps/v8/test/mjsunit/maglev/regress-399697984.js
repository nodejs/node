// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --min-maglev-inlining-frequency=0

function bar() {}
function foo() {
    var x = {};
    x.__defineSetter__('a', bar);
    x.a = 1;
}
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
