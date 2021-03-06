// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt


function foo(i, deopt = false, deoptobj = null) {
    if (i == 0) {
        if (deopt) {
            // Trigger a soft deopt.
            deoptobj.bar();
        }
    } else {
        foo(i - 1, deopt, deoptobj);
    }
}

%PrepareFunctionForOptimization(foo);
foo(10);
foo(10);
%OptimizeFunctionOnNextCall(foo);
foo(10);

assertOptimized(foo);

foo(10, true, { bar: function(){} });

assertUnoptimized(foo);
