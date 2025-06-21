// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The GetIterator bytecode is used to implement a part of the iterator
// protocol (https://tc39.es/ecma262/#sec-getiterator). Here, the
// bytecode performs multiple operations including some that have side-effects
// and may deoptimize eagerly or lazily.
// This test ensures the call @@iterator lazy deoptimization is handled correctly.

// Flags: --allow-natives-syntax --no-always-turbofan

let getIteratorCount = 0;
let iteratorCount = 0;
let iteratorAfterEagerDeoptCount = 0;
let triggerLazyDeopt = false;

function foo(obj) {
  // The following for-of loop uses the iterator protocol to iterate
  // over the 'obj'.
  // The GetIterator bytecode involves 3 steps:
  // 1. method = GetMethod(obj, @@iterator)
  // 2. iterator = Call(method, obj).
  // 3. if(!IsJSReceiver(iterator)) throw SymbolIteratorInvalid.
  for (var x of obj) {
  }
}

// The lazy deoptimization is triggerred by setting the
// 'triggerLazyDeopt' to true. And the lazy deopt should
// goto continuation to check the call result is JSReceiver.
var iterator =
  function () {
    if (triggerLazyDeopt) {
      iteratorAfterEagerDeoptCount++;
      %DeoptimizeFunction(foo);
      // SymbolIteratorInvalid should be throwed.
      return 1;
    }
    iteratorCount++;
    return {
      next: function () {
        return { done: true };
      }
    }
  }

let y = {
  get [Symbol.iterator]() {
    getIteratorCount++;
    return iterator;
  }
};

%PrepareFunctionForOptimization(foo);
foo(y);
foo(y);
%OptimizeFunctionOnNextCall(foo);
triggerLazyDeopt = true;
assertThrows(() => {
  foo(y)
}, TypeError, "Result of the Symbol.iterator method is not an object")
assertUnoptimized(foo);
assertEquals(getIteratorCount, 3);
assertEquals(iteratorCount, 2);
assertEquals(iteratorAfterEagerDeoptCount, 1);
