// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev


function foo(g) {
    var obj = {};
    obj.prop0 = function () {
      try {
        if(g) __call_nonexisting_function();
        return 3.1;
      } catch (e) {}
    }();

    return obj;
}

%PrepareFunctionForOptimization(foo);
foo(false);
foo(false);
%OptimizeMaglevOnNextCall(foo);
foo(false);
assertOptimized(foo);
foo(true);
// foo needs to trigger a deopt because we see an undefined for the HeapNumber
// field.
assertUnoptimized(foo);
