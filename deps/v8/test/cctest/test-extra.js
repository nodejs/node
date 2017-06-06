// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function (global, binding, v8) {
  'use strict';
  binding.testExtraShouldReturnFive = function() {
    return 5;
  };

  binding.testExtraShouldCallToRuntime = function() {
    return binding.runtime(3);
  };

  binding.testFunctionToString = function() {
    function foo() { return 1; }
    return foo.toString();
  };

  binding.testStackTrace = function(f) {
    return f();
  }

  // Exercise all of the extras utils:
  // - v8.createPrivateSymbol
  // - v8.simpleBind, v8.uncurryThis
  // - v8.InternalPackedArray
  // - v8.createPromise, v8.resolvePromise, v8.rejectPromise

  const Object = global.Object;
  const hasOwn = v8.uncurryThis(Object.prototype.hasOwnProperty);

  const Function = global.Function;
  const call = v8.uncurryThis(Function.prototype.call);
  const apply = v8.uncurryThis(Function.prototype.apply);

  const Promise = global.Promise;
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);

  const arrayToTest = new v8.InternalPackedArray();
  arrayToTest.push(1);
  arrayToTest.push(2);
  arrayToTest.pop();
  arrayToTest.unshift("a", "b", "c");
  arrayToTest.shift();
  arrayToTest.splice(0, 1);
  const slicedArray = arrayToTest.slice();
  const arraysOK = arrayToTest.length === 2 && arrayToTest[0] === "c" &&
      arrayToTest[1] === 1 && slicedArray.length === 2 &&
      slicedArray[0] === "c" && slicedArray[1] === 1;

  binding.testExtraCanUseUtils = function() {
    const fulfilledPromise = v8.createPromise();
    v8.resolvePromise(
      fulfilledPromise,
      hasOwn({ test: 'test' }, 'test') ? 1 : -1,
      undefined  // pass an extra arg to test arguments adapter frame
    );

    const fulfilledPromise2 = Promise_resolve(call(function (arg1, arg2) {
      return (this.prop === arg1 && arg1 === 'value' && arg2) ? 2 : -1;
    }, { prop: 'value' }, 'value', arraysOK));

    const rejectedPromise = v8.createPromise();
    v8.rejectPromise(rejectedPromise, apply(function (arg1, arg2) {
      return (arg1 === arg2 && arg2 === 'x') ? 3 : -1;
    }, null, new v8.InternalPackedArray('x', 'x')));

    const rejectedButHandledPromise = v8.createPromise();
    v8.rejectPromise(rejectedButHandledPromise, 4);
    v8.markPromiseAsHandled(rejectedButHandledPromise);

    function promiseStateToString(promise) {
      switch (v8.promiseState(promise)) {
        case v8.kPROMISE_PENDING:
          return "pending";
        case v8.kPROMISE_FULFILLED:
          return "fulfilled";
        case v8.kPROMISE_REJECTED:
          return "rejected";
        default:
          throw new Error("Unexpected value for promiseState");
      }
    }

    let promiseStates = promiseStateToString(new Promise(() => {})) + ' ' +
                        promiseStateToString(fulfilledPromise) + ' ' +
                        promiseStateToString(rejectedPromise);

    return {
      privateSymbol: v8.createPrivateSymbol('sym'),
      fulfilledPromise, // should be fulfilled with 1
      fulfilledPromise2, // should be fulfilled with 2
      rejectedPromise, // should be rejected with 3
      rejectedButHandledPromise, // should be rejected but have a handler
      promiseStates, // should be the string "pending fulfilled rejected"
      promiseIsPromise: v8.isPromise(fulfilledPromise), // should be true
      thenableIsPromise: v8.isPromise({ then() { } })  // should be false
    };
  };
})
