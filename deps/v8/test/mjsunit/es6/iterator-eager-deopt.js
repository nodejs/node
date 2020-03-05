// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GetIterator bytecode is used to implement a part of the iterator
// protocol (https://tc39.es/ecma262/#sec-getiterator). Here, the
// bytecode performs multiple operations including some that have side-effects
// and may deoptimize eagerly or lazily.
// This test ensures the eager deoptimization is handled correctly.

// Flags: --allow-natives-syntax --no-always-opt

var getIteratorCount = 0;
var iteratorCount = 0;
var iteratorAfterEagerDeoptCount = 0;

function foo(obj) {
    // The following for-of loop uses the iterator protocol to iterate
    // over the 'obj'.
    // The GetIterator bytecode invovlves 2 steps:
    // 1. method = GetMethod(obj, @@iterator)
    // 2. iterator = Call(method, obj).
    for(var x of obj){}
}

// This iterator gets inlined when the 'foo' function is JIT compiled for
// the first time.
var iterator = function() {
    iteratorCount++;
    return {
        next: function() {
            return { done: true };
        }
    }
}

var iteratorAfterEagerDeopt = function() {
    iteratorAfterEagerDeoptCount++;
    return {
        next: function() {
            return { done: true };
        }
    }
}

// Here, retrieval of function at @@iterator has side effect (increments the
// 'getIteratorCount'). Changing the value of 'iterator' in the JIT compiled
// 'foo' causes deoptimization after the count is incremented. Now the deopt
// cannot resume at the beginning of the bytecode because it would end up in
// incrementing the count again.
let y = { get [Symbol.iterator] () {
            getIteratorCount++;
            return iterator;
         }
    };

%PrepareFunctionForOptimization(foo);
foo(y);
foo(y);
%OptimizeFunctionOnNextCall(foo);

// Change the value of 'iterator' to trigger eager deoptimization of 'foo'.
iterator = iteratorAfterEagerDeopt
foo(y);
assertUnoptimized(foo);
assertEquals(getIteratorCount, 3);
assertEquals(iteratorCount, 2);
assertEquals(iteratorAfterEagerDeoptCount, 1);
