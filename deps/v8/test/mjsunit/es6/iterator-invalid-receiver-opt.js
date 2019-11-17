// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GetIterator bytecode is used to implement a part of the iterator
// protocol (https://tc39.es/ecma262/#sec-getiterator).
// Here, call to the @@iterator property returns invalid JS receiver.
// This test ensures that the optimized version of the GetIterator bytecode
// incorporates exception handling mechanism without deoptimizing.

// Flags: --allow-natives-syntax --opt

var iteratorCount = 0;
var exceptionCount = 0;

function foo(obj) {
    // The following for-of loop uses the iterator protocol to iterate
    // over the 'obj'.
    // The GetIterator bytecode invovlves 3 steps:
    // 1. method = GetMethod(obj, @@iterator)
    // 2. iterator = Call(method, obj)
    // 3. if(!IsJSReceiver(iterator)) throw SymbolIteratorInvalid.
     try{
        for(let a of obj){
            assertUnreachable();
        }
    } catch(e){
        exceptionCount++;
    }
}

// This iterator retuns '3' which is not a valid JSReceiver
var iterator = function() {
    iteratorCount++;
    return 3;
}

let y = {
    get [Symbol.iterator]() {
       return iterator;
    }
};

%PrepareFunctionForOptimization(foo);
foo(y);
foo(y);
%OptimizeFunctionOnNextCall(foo);
foo(y);
assertOptimized(foo);
assertEquals(iteratorCount, 3);
assertEquals(exceptionCount, 3);
