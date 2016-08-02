// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var InternalArray = utils.InternalArray;
var MakeTypeError;
var promiseCombinedDeferredSymbol =
    utils.ImportNow("promise_combined_deferred_symbol");
var promiseHasHandlerSymbol =
    utils.ImportNow("promise_has_handler_symbol");
var promiseRejectReactionsSymbol =
    utils.ImportNow("promise_reject_reactions_symbol");
var promiseFulfillReactionsSymbol =
    utils.ImportNow("promise_fulfill_reactions_symbol");
var promiseRawSymbol = utils.ImportNow("promise_raw_symbol");
var promiseStateSymbol = utils.ImportNow("promise_state_symbol");
var promiseResultSymbol = utils.ImportNow("promise_result_symbol");
var SpeciesConstructor;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
  SpeciesConstructor = from.SpeciesConstructor;
});

// -------------------------------------------------------------------

// [[PromiseState]] values:
const kPending = 0;
const kFulfilled = +1;
const kRejected = -1;

var lastMicrotaskId = 0;

// ES#sec-createresolvingfunctions
// CreateResolvingFunctions ( promise )
function CreateResolvingFunctions(promise) {
  var alreadyResolved = false;

  // ES#sec-promise-resolve-functions
  // Promise Resolve Functions
  var resolve = value => {
    if (alreadyResolved === true) return;
    alreadyResolved = true;
    FulfillPromise(promise, value);
  };

  // ES#sec-promise-reject-functions
  // Promise Reject Functions
  var reject = reason => {
    if (alreadyResolved === true) return;
    alreadyResolved = true;
    RejectPromise(promise, reason);
  };

  return {
    __proto__: null,
    resolve: resolve,
    reject: reject
  };
}


// ES#sec-promise-executor
// Promise ( executor )
var GlobalPromise = function Promise(resolver) {
  if (resolver === promiseRawSymbol) {
    return %_NewObject(GlobalPromise, new.target);
  }
  if (IS_UNDEFINED(new.target)) throw MakeTypeError(kNotAPromise, this);
  if (!IS_CALLABLE(resolver)) {
    throw MakeTypeError(kResolverNotAFunction, resolver);
  }

  var promise = PromiseInit(%_NewObject(GlobalPromise, new.target));
  var callbacks = CreateResolvingFunctions(promise);

  try {
    %DebugPushPromise(promise, Promise);
    resolver(callbacks.resolve, callbacks.reject);
  } catch (e) {
    %_Call(callbacks.reject, UNDEFINED, e);
  } finally {
    %DebugPopPromise();
  }

  return promise;
}

// Core functionality.

function PromiseSet(promise, status, value, onResolve, onReject) {
  SET_PRIVATE(promise, promiseStateSymbol, status);
  SET_PRIVATE(promise, promiseResultSymbol, value);
  SET_PRIVATE(promise, promiseFulfillReactionsSymbol, onResolve);
  SET_PRIVATE(promise, promiseRejectReactionsSymbol, onReject);
  return promise;
}

function PromiseCreateAndSet(status, value) {
  var promise = new GlobalPromise(promiseRawSymbol);
  // If debug is active, notify about the newly created promise first.
  if (DEBUG_IS_ACTIVE) PromiseSet(promise, kPending, UNDEFINED);
  return PromiseSet(promise, status, value);
}

function PromiseInit(promise) {
  return PromiseSet(
      promise, kPending, UNDEFINED, new InternalArray, new InternalArray)
}

function PromiseDone(promise, status, value, promiseQueue) {
  if (GET_PRIVATE(promise, promiseStateSymbol) === kPending) {
    var tasks = GET_PRIVATE(promise, promiseQueue);
    if (tasks.length) PromiseEnqueue(value, tasks, status);
    PromiseSet(promise, status, value);
  }
}

function PromiseHandle(value, handler, deferred) {
  try {
    %DebugPushPromise(deferred.promise, PromiseHandle);
    var result = handler(value);
    deferred.resolve(result);
  } catch (exception) {
    try { deferred.reject(exception); } catch (e) { }
  } finally {
    %DebugPopPromise();
  }
}

function PromiseEnqueue(value, tasks, status) {
  var id, name, instrumenting = DEBUG_IS_ACTIVE;
  %EnqueueMicrotask(function() {
    if (instrumenting) {
      %DebugAsyncTaskEvent({ type: "willHandle", id: id, name: name });
    }
    for (var i = 0; i < tasks.length; i += 2) {
      PromiseHandle(value, tasks[i], tasks[i + 1])
    }
    if (instrumenting) {
      %DebugAsyncTaskEvent({ type: "didHandle", id: id, name: name });
    }
  });
  if (instrumenting) {
    id = ++lastMicrotaskId;
    name = status === kFulfilled ? "Promise.resolve" : "Promise.reject";
    %DebugAsyncTaskEvent({ type: "enqueue", id: id, name: name });
  }
}

function PromiseIdResolveHandler(x) { return x }
function PromiseIdRejectHandler(r) { throw r }

function PromiseNopResolver() {}

// -------------------------------------------------------------------
// Define exported functions.

// For bootstrapper.

// ES#sec-ispromise IsPromise ( x )
function IsPromise(x) {
  return IS_RECEIVER(x) && HAS_DEFINED_PRIVATE(x, promiseStateSymbol);
}

function PromiseCreate() {
  return new GlobalPromise(PromiseNopResolver)
}

// ES#sec-fulfillpromise
// FulfillPromise ( promise, value)
function FulfillPromise(promise, x) {
  if (x === promise) {
    return RejectPromise(promise, MakeTypeError(kPromiseCyclic, x));
  }
  if (IS_RECEIVER(x)) {
    // 25.4.1.3.2 steps 8-12
    try {
      var then = x.then;
    } catch (e) {
      return RejectPromise(promise, e);
    }
    if (IS_CALLABLE(then)) {
      // PromiseResolveThenableJob
      var id, name, instrumenting = DEBUG_IS_ACTIVE;
      %EnqueueMicrotask(function() {
        if (instrumenting) {
          %DebugAsyncTaskEvent({ type: "willHandle", id: id, name: name });
        }
        var callbacks = CreateResolvingFunctions(promise);
        try {
          %_Call(then, x, callbacks.resolve, callbacks.reject);
        } catch (e) {
          %_Call(callbacks.reject, UNDEFINED, e);
        }
        if (instrumenting) {
          %DebugAsyncTaskEvent({ type: "didHandle", id: id, name: name });
        }
      });
      if (instrumenting) {
        id = ++lastMicrotaskId;
        name = "PromseResolveThenableJob";
        %DebugAsyncTaskEvent({ type: "enqueue", id: id, name: name });
      }
      return;
    }
  }
  PromiseDone(promise, kFulfilled, x, promiseFulfillReactionsSymbol);
}

// ES#sec-rejectpromise
// RejectPromise ( promise, reason )
function RejectPromise(promise, r) {
  // Check promise status to confirm that this reject has an effect.
  // Call runtime for callbacks to the debugger or for unhandled reject.
  if (GET_PRIVATE(promise, promiseStateSymbol) === kPending) {
    var debug_is_active = DEBUG_IS_ACTIVE;
    if (debug_is_active ||
        !HAS_DEFINED_PRIVATE(promise, promiseHasHandlerSymbol)) {
      %PromiseRejectEvent(promise, r, debug_is_active);
    }
  }
  PromiseDone(promise, kRejected, r, promiseRejectReactionsSymbol)
}

// ES#sec-newpromisecapability
// NewPromiseCapability ( C )
function NewPromiseCapability(C) {
  if (C === GlobalPromise) {
    // Optimized case, avoid extra closure.
    var promise = PromiseInit(new GlobalPromise(promiseRawSymbol));
    var callbacks = CreateResolvingFunctions(promise);
    return {
      promise: promise,
      resolve: callbacks.resolve,
      reject: callbacks.reject
    };
  }

  var result = {promise: UNDEFINED, resolve: UNDEFINED, reject: UNDEFINED };
  result.promise = new C((resolve, reject) => {
    if (!IS_UNDEFINED(result.resolve) || !IS_UNDEFINED(result.reject))
        throw MakeTypeError(kPromiseExecutorAlreadyInvoked);
    result.resolve = resolve;
    result.reject = reject;
  });

  if (!IS_CALLABLE(result.resolve) || !IS_CALLABLE(result.reject))
      throw MakeTypeError(kPromiseNonCallable);

  return result;
}

// Unspecified V8-specific legacy function
function PromiseDefer() {
  %IncrementUseCounter(kPromiseDefer);
  return NewPromiseCapability(this);
}

// Unspecified V8-specific legacy function
function PromiseAccept(x) {
  %IncrementUseCounter(kPromiseAccept);
  return %_Call(PromiseResolve, this, x);
}

// ES#sec-promise.reject
// Promise.reject ( x )
function PromiseReject(r) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, PromiseResolve);
  }
  if (this === GlobalPromise) {
    // Optimized case, avoid extra closure.
    var promise = PromiseCreateAndSet(kRejected, r);
    // The debug event for this would always be an uncaught promise reject,
    // which is usually simply noise. Do not trigger that debug event.
    %PromiseRejectEvent(promise, r, false);
    return promise;
  } else {
    var promiseCapability = NewPromiseCapability(this);
    %_Call(promiseCapability.reject, UNDEFINED, r);
    return promiseCapability.promise;
  }
}

// Shortcut Promise.reject and Promise.resolve() implementations, used by
// Async Functions implementation.
function PromiseCreateRejected(r) {
  return %_Call(PromiseReject, GlobalPromise, r);
}

function PromiseCreateResolved(x) {
  return %_Call(PromiseResolve, GlobalPromise, x);
}

// ES#sec-promise.prototype.then
// Promise.prototype.then ( onFulfilled, onRejected )
// Multi-unwrapped chaining with thenable coercion.
function PromiseThen(onResolve, onReject) {
  var status = GET_PRIVATE(this, promiseStateSymbol);
  if (IS_UNDEFINED(status)) {
    throw MakeTypeError(kNotAPromise, this);
  }

  var constructor = SpeciesConstructor(this, GlobalPromise);
  onResolve = IS_CALLABLE(onResolve) ? onResolve : PromiseIdResolveHandler;
  onReject = IS_CALLABLE(onReject) ? onReject : PromiseIdRejectHandler;
  var deferred = NewPromiseCapability(constructor);
  switch (status) {
    case kPending:
      GET_PRIVATE(this, promiseFulfillReactionsSymbol).push(onResolve,
                                                            deferred);
      GET_PRIVATE(this, promiseRejectReactionsSymbol).push(onReject, deferred);
      break;
    case kFulfilled:
      PromiseEnqueue(GET_PRIVATE(this, promiseResultSymbol),
                     [onResolve, deferred],
                     kFulfilled);
      break;
    case kRejected:
      if (!HAS_DEFINED_PRIVATE(this, promiseHasHandlerSymbol)) {
        // Promise has already been rejected, but had no handler.
        // Revoke previously triggered reject event.
        %PromiseRevokeReject(this);
      }
      PromiseEnqueue(GET_PRIVATE(this, promiseResultSymbol),
                     [onReject, deferred],
                     kRejected);
      break;
  }
  // Mark this promise as having handler.
  SET_PRIVATE(this, promiseHasHandlerSymbol, true);
  return deferred.promise;
}

// Unspecified V8-specific legacy function
// Chain is left around for now as an alias for then
function PromiseChain(onResolve, onReject) {
  %IncrementUseCounter(kPromiseChain);
  return %_Call(PromiseThen, this, onResolve, onReject);
}

// ES#sec-promise.prototype.catch
// Promise.prototype.catch ( onRejected )
function PromiseCatch(onReject) {
  return this.then(UNDEFINED, onReject);
}

// Combinators.

// ES#sec-promise.resolve
// Promise.resolve ( x )
function PromiseResolve(x) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, PromiseResolve);
  }
  if (IsPromise(x) && x.constructor === this) return x;

  var promiseCapability = NewPromiseCapability(this);
  var resolveResult = %_Call(promiseCapability.resolve, UNDEFINED, x);
  return promiseCapability.promise;
}

// ES#sec-promise.all
// Promise.all ( iterable )
function PromiseAll(iterable) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, "Promise.all");
  }

  var deferred = NewPromiseCapability(this);
  var resolutions = new InternalArray();
  var count;

  function CreateResolveElementFunction(index, values, promiseCapability) {
    var alreadyCalled = false;
    return (x) => {
      if (alreadyCalled === true) return;
      alreadyCalled = true;
      values[index] = x;
      if (--count === 0) {
        var valuesArray = [];
        %MoveArrayContents(values, valuesArray);
        %_Call(promiseCapability.resolve, UNDEFINED, valuesArray);
      }
    };
  }

  try {
    var i = 0;
    count = 1;
    for (var value of iterable) {
      var nextPromise = this.resolve(value);
      ++count;
      nextPromise.then(
          CreateResolveElementFunction(i, resolutions, deferred),
          deferred.reject);
      SET_PRIVATE(deferred.reject, promiseCombinedDeferredSymbol, deferred);
      ++i;
    }

    // 6.d
    if (--count === 0) {
      var valuesArray = [];
      %MoveArrayContents(resolutions, valuesArray);
      %_Call(deferred.resolve, UNDEFINED, valuesArray);
    }

  } catch (e) {
    %_Call(deferred.reject, UNDEFINED, e);
  }
  return deferred.promise;
}

// ES#sec-promise.race
// Promise.race ( iterable )
function PromiseRace(iterable) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, PromiseRace);
  }

  var deferred = NewPromiseCapability(this);
  try {
    for (var value of iterable) {
      this.resolve(value).then(deferred.resolve, deferred.reject);
      SET_PRIVATE(deferred.reject, promiseCombinedDeferredSymbol, deferred);
    }
  } catch (e) {
    deferred.reject(e)
  }
  return deferred.promise;
}


// Utility for debugger

function PromiseHasUserDefinedRejectHandlerRecursive(promise) {
  var queue = GET_PRIVATE(promise, promiseRejectReactionsSymbol);
  if (IS_UNDEFINED(queue)) return false;
  for (var i = 0; i < queue.length; i += 2) {
    var handler = queue[i];
    if (handler !== PromiseIdRejectHandler) {
      var deferred = GET_PRIVATE(handler, promiseCombinedDeferredSymbol);
      if (IS_UNDEFINED(deferred)) return true;
      if (PromiseHasUserDefinedRejectHandlerRecursive(deferred.promise)) {
        return true;
      }
    } else if (PromiseHasUserDefinedRejectHandlerRecursive(
                   queue[i + 1].promise)) {
      return true;
    }
  }
  return false;
}

// Return whether the promise will be handled by a user-defined reject
// handler somewhere down the promise chain. For this, we do a depth-first
// search for a reject handler that's not the default PromiseIdRejectHandler.
function PromiseHasUserDefinedRejectHandler() {
  return PromiseHasUserDefinedRejectHandlerRecursive(this);
};

// -------------------------------------------------------------------
// Install exported functions.

%AddNamedProperty(global, 'Promise', GlobalPromise, DONT_ENUM);
%AddNamedProperty(GlobalPromise.prototype, toStringTagSymbol, "Promise",
                  DONT_ENUM | READ_ONLY);

utils.InstallFunctions(GlobalPromise, DONT_ENUM, [
  "reject", PromiseReject,
  "all", PromiseAll,
  "race", PromiseRace,
  "resolve", PromiseResolve
]);

utils.InstallFunctions(GlobalPromise.prototype, DONT_ENUM, [
  "then", PromiseThen,
  "catch", PromiseCatch
]);

%InstallToContext([
  "promise_catch", PromiseCatch,
  "promise_chain", PromiseChain,
  "promise_create", PromiseCreate,
  "promise_has_user_defined_reject_handler", PromiseHasUserDefinedRejectHandler,
  "promise_reject", RejectPromise,
  "promise_resolve", FulfillPromise,
  "promise_then", PromiseThen,
  "promise_create_rejected", PromiseCreateRejected,
  "promise_create_resolved", PromiseCreateResolved
]);

// This allows extras to create promises quickly without building extra
// resolve/reject closures, and allows them to later resolve and reject any
// promise without having to hold on to those closures forever.
utils.InstallFunctions(extrasUtils, 0, [
  "createPromise", PromiseCreate,
  "resolvePromise", FulfillPromise,
  "rejectPromise", RejectPromise
]);

// TODO(v8:4567): Allow experimental natives to remove function prototype
[PromiseChain, PromiseDefer, PromiseAccept].forEach(
    fn => %FunctionRemovePrototype(fn));

utils.Export(function(to) {
  to.PromiseChain = PromiseChain;
  to.PromiseDefer = PromiseDefer;
  to.PromiseAccept = PromiseAccept;

  to.PromiseCreateRejected = PromiseCreateRejected;
  to.PromiseCreateResolved = PromiseCreateResolved;
  to.PromiseThen = PromiseThen;
});

})
