// Copyright 2015 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Implementation of functions that are shared between ReadableStream and
// WritableStream.

(function(global, binding, v8) {
  'use strict';

  // Common private symbols. These correspond directly to internal slots in the
  // standard. "[[X]]" in the standard is spelt _X here.
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _queueTotalSize = v8.createPrivateSymbol('[[queueTotalSize]]');

  // Javascript functions. It is important to use these copies for security and
  // robustness. See "V8 Extras Design Doc", section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const Boolean = global.Boolean;
  const Number = global.Number;
  const Number_isFinite = Number.isFinite;
  const Number_isNaN = Number.isNaN;

  const RangeError = global.RangeError;
  const TypeError = global.TypeError;

  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);

  function hasOwnPropertyNoThrow(x, property) {
    // The cast of |x| to Boolean will eliminate undefined and null, which would
    // cause hasOwnProperty to throw a TypeError, as well as some other values
    // that can't be objects and so will fail the check anyway.
    return Boolean(x) && hasOwnProperty(x, property);
  }

  //
  // Assert is not normally enabled, to avoid the space and time overhead. To
  // enable, uncomment this definition and then in the file you wish to enable
  // asserts for, uncomment the assert statements and add this definition:
  // const assert = pred => binding.SimpleAssert(pred);
  //
  // binding.SimpleAssert = pred => {
  //   if (pred) {
  //     return;
  //   }
  //   v8.log('\n\n\n  *** ASSERTION FAILURE ***\n\n');
  //   v8.logStackTrace();
  //   v8.log('**************************************************\n\n');
  //   class StreamsAssertionError extends Error {}
  //   throw new StreamsAssertionError('Streams Assertion Failure');
  // };

  //
  // Promise-manipulation functions
  //

  // Not exported.
  function streamInternalError() {
    throw new RangeError('Stream API Internal Error');
  }

  function rejectPromise(p, reason) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.rejectPromise(p, reason);
  }

  function resolvePromise(p, value) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.resolvePromise(p, value);
  }

  function markPromiseAsHandled(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.markPromiseAsHandled(p);
  }

  function promiseState(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    return v8.promiseState(p);
  }

  //
  // Queue-with-Sizes Operations
  //
  function DequeueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].shift();
    container[_queueTotalSize] -= pair.size;
    if (container[_queueTotalSize] < 0) {
      container[_queueTotalSize] = 0;
    }
    return pair.value;
  }

  function EnqueueValueWithSize(container, value, size) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal 'slots.');
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(binding.streamErrors.invalidSize);
    }

    container[_queue].push({ value, size });
    container[_queueTotalSize] += size;
  }

  function PeekQueueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].peek();
    return pair.value;
  }

  function ResetQueue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    container[_queue] = new binding.SimpleQueue();
    container[_queueTotalSize] = 0;
  }

  // Not exported.
  function IsFiniteNonNegativeNumber(v) {
    return Number_isFinite(v) && v >= 0;
  }

  function ValidateAndNormalizeHighWaterMark(highWaterMark) {
    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }
    return highWaterMark;
  }

  // Unlike the version in the standard, this implementation returns the
  // original function as-is if it is set. This means users of the return value
  // need to be careful to explicitly set |this| when calling it.
  function MakeSizeAlgorithmFromSizeFunction(size) {
    if (size === undefined) {
      return () => 1;
    }

    if (typeof size !== 'function') {
      throw new TypeError(binding.streamErrors.sizeNotAFunction);
    }

    return size;
  }

  //
  // Invoking functions.
  // These differ from the Invoke versions in the spec in that they take a fixed
  // number of arguments rather than a list, and also take a name to be used for
  // the function on error.
  //

  // Internal utility functions. Not exported.
  const callFunction = v8.uncurryThis(global.Function.prototype.call);
  const errTmplMustBeFunctionOrUndefined = (name) =>
    `${name} must be a function or undefined`;
  const Promise_resolve = global.Promise.resolve.bind(global.Promise);
  const Promise_reject = global.Promise.reject.bind(global.Promise);
  const Function_bind = v8.uncurryThis(global.Function.prototype.bind);

  function resolveMethod(O, P, nameForError) {
    const method = O[P];

    if (typeof method !== 'function' && typeof method !== 'undefined') {
      throw new TypeError(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    return method;
  }

  function CreateAlgorithmFromUnderlyingMethod(
    underlyingObject, methodName, algoArgCount, methodNameForError) {
    // assert(underlyingObject !== undefined,
    //        'underlyingObject is not undefined.');
    // assert(IsPropertyKey(methodName),
    // '! IsPropertyKey(methodName) is true.');
    // assert(algoArgCount === 0 || algoArgCount === 1,
    // 'algoArgCount is 0 or 1.');
    // assert(
    //     typeof methodNameForError === 'string',
    //     'methodNameForError is a string');
    const method =
        resolveMethod(underlyingObject, methodName, methodNameForError);
    // The implementation uses bound functions rather than lambdas where
    // possible to give the compiler the maximum opportunity to optimise.
    if (method === undefined) {
      return () => Promise_resolve();
    }

    if (algoArgCount === 0) {
      return Function_bind(PromiseCall0, undefined, method, underlyingObject);
    }

    return Function_bind(PromiseCall1, undefined, method, underlyingObject);
  }

  function CreateAlgorithmFromUnderlyingMethodPassingController(
    underlyingObject, methodName, algoArgCount, controller,
    methodNameForError) {
    // assert(underlyingObject !== undefined,
    //        'underlyingObject is not undefined.');
    // assert(IsPropertyKey(methodName),
    // '! IsPropertyKey(methodName) is true.');
    // assert(algoArgCount === 0 || algoArgCount === 1,
    // 'algoArgCount is 0 or 1.');
    // assert(typeof controller === 'object', 'controller is an object');
    // assert(
    //     typeof methodNameForError === 'string',
    //     'methodNameForError is a string');
    const method =
        resolveMethod(underlyingObject, methodName, methodNameForError);
    if (method === undefined) {
      return () => Promise_resolve();
    }

    if (algoArgCount === 0) {
      return Function_bind(
        PromiseCall1, undefined, method, underlyingObject, controller);
    }

    return (arg) => PromiseCall2(method, underlyingObject, arg, controller);
  }

  // Modified from InvokeOrNoop in spec. Takes 1 argument.
  function CallOrNoop1(O, P, arg0, nameForError) {
    const method = resolveMethod(O, P, nameForError);
    if (method === undefined) {
      return undefined;
    }

    return callFunction(method, O, arg0);
  }

  function PromiseCall0(F, V) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function PromiseCall1(F, V, arg0) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V, arg0));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function PromiseCall2(F, V, arg0, arg1) {
    // assert(typeof F === 'function', 'IsCallable(F) is true.');
    // assert(V !== undefined, 'V is not undefined.');
    try {
      return Promise_resolve(callFunction(F, V, arg0, arg1));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  binding.streamOperations = {
    _queue,
    _queueTotalSize,
    hasOwnPropertyNoThrow,
    rejectPromise,
    resolvePromise,
    markPromiseAsHandled,
    promiseState,
    CreateAlgorithmFromUnderlyingMethod,
    CreateAlgorithmFromUnderlyingMethodPassingController,
    DequeueValue,
    EnqueueValueWithSize,
    PeekQueueValue,
    ResetQueue,
    ValidateAndNormalizeHighWaterMark,
    MakeSizeAlgorithmFromSizeFunction,
    CallOrNoop1,
    PromiseCall2
  };
});
