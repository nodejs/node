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

  binding.testExtraCanUseUtils = function() {
    const fulfilledPromise = v8.createPromise();
    v8.resolvePromise(
      fulfilledPromise,
      hasOwn({ test: 'test' }, 'test') ? 1 : -1
    );

    const fulfilledPromise2 = Promise_resolve(call(function (arg1) {
      return (this.prop === arg1 && arg1 === 'value') ? 2 : -1;
    }, { prop: 'value' }, 'value'));

    const rejectedPromise = v8.createPromise();
    v8.rejectPromise(rejectedPromise, apply(function (arg1, arg2) {
      return (arg1 === arg2 && arg2 === 'x') ? 3 : -1;
    }, null, new v8.InternalPackedArray('x', 'x')));

    return {
      privateSymbol: v8.createPrivateSymbol('sym'),
      fulfilledPromise, // should be fulfilled with 1
      fulfilledPromise2, // should be fulfilled with 2
      rejectedPromise // should be rejected with 3
    };
  };
})
