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
var promiseOnRejectSymbol = utils.ImportNow("promise_on_reject_symbol");
var promiseOnResolveSymbol =
    utils.ImportNow("promise_on_resolve_symbol");
var promiseRawSymbol = utils.ImportNow("promise_raw_symbol");
var promiseStatusSymbol = utils.ImportNow("promise_status_symbol");
var promiseValueSymbol = utils.ImportNow("promise_value_symbol");
var SpeciesConstructor;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeTypeError = from.MakeTypeError;
  SpeciesConstructor = from.SpeciesConstructor;
});

// -------------------------------------------------------------------

// Status values: 0 = pending, +1 = resolved, -1 = rejected
var lastMicrotaskId = 0;

function CreateResolvingFunctions(promise) {
  var alreadyResolved = false;

  var resolve = value => {
    if (alreadyResolved === true) return;
    alreadyResolved = true;
    PromiseResolve(promise, value);
  };

  var reject = reason => {
    if (alreadyResolved === true) return;
    alreadyResolved = true;
    PromiseReject(promise, reason);
  };

  return {
    __proto__: null,
    resolve: resolve,
    reject: reject
  };
}


var GlobalPromise = function Promise(resolver) {
  if (resolver === promiseRawSymbol) {
    return %NewObject(GlobalPromise, new.target);
  }
  if (IS_UNDEFINED(new.target)) throw MakeTypeError(kNotAPromise, this);
  if (!IS_CALLABLE(resolver))
    throw MakeTypeError(kResolverNotAFunction, resolver);

  var promise = PromiseInit(%NewObject(GlobalPromise, new.target));
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
  SET_PRIVATE(promise, promiseStatusSymbol, status);
  SET_PRIVATE(promise, promiseValueSymbol, value);
  SET_PRIVATE(promise, promiseOnResolveSymbol, onResolve);
  SET_PRIVATE(promise, promiseOnRejectSymbol, onReject);
  if (DEBUG_IS_ACTIVE) {
    %DebugPromiseEvent({ promise: promise, status: status, value: value });
  }
  return promise;
}

function PromiseCreateAndSet(status, value) {
  var promise = new GlobalPromise(promiseRawSymbol);
  // If debug is active, notify about the newly created promise first.
  if (DEBUG_IS_ACTIVE) PromiseSet(promise, 0, UNDEFINED);
  return PromiseSet(promise, status, value);
}

function PromiseInit(promise) {
  return PromiseSet(
      promise, 0, UNDEFINED, new InternalArray, new InternalArray)
}

function PromiseDone(promise, status, value, promiseQueue) {
  if (GET_PRIVATE(promise, promiseStatusSymbol) === 0) {
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
    name = status > 0 ? "Promise.resolve" : "Promise.reject";
    %DebugAsyncTaskEvent({ type: "enqueue", id: id, name: name });
  }
}

function PromiseIdResolveHandler(x) { return x }
function PromiseIdRejectHandler(r) { throw r }

function PromiseNopResolver() {}

// -------------------------------------------------------------------
// Define exported functions.

// For bootstrapper.

function IsPromise(x) {
  return IS_RECEIVER(x) && HAS_DEFINED_PRIVATE(x, promiseStatusSymbol);
}

function PromiseCreate() {
  return new GlobalPromise(PromiseNopResolver)
}

function PromiseResolve(promise, x) {
  if (x === promise) {
    return PromiseReject(promise, MakeTypeError(kPromiseCyclic, x));
  }
  if (IS_RECEIVER(x)) {
    // 25.4.1.3.2 steps 8-12
    try {
      var then = x.then;
    } catch (e) {
      return PromiseReject(promise, e);
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
  PromiseDone(promise, +1, x, promiseOnResolveSymbol);
}

function PromiseReject(promise, r) {
  // Check promise status to confirm that this reject has an effect.
  // Call runtime for callbacks to the debugger or for unhandled reject.
  if (GET_PRIVATE(promise, promiseStatusSymbol) == 0) {
    var debug_is_active = DEBUG_IS_ACTIVE;
    if (debug_is_active ||
        !HAS_DEFINED_PRIVATE(promise, promiseHasHandlerSymbol)) {
      %PromiseRejectEvent(promise, r, debug_is_active);
    }
  }
  PromiseDone(promise, -1, r, promiseOnRejectSymbol)
}

// Convenience.

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

  return result;
}

function PromiseDeferred() {
  %IncrementUseCounter(kPromiseDefer);
  return NewPromiseCapability(this);
}

function PromiseResolved(x) {
  %IncrementUseCounter(kPromiseAccept);
  return %_Call(PromiseCast, this, x);
}

function PromiseRejected(r) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, PromiseRejected);
  }
  if (this === GlobalPromise) {
    // Optimized case, avoid extra closure.
    var promise = PromiseCreateAndSet(-1, r);
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

// Multi-unwrapped chaining with thenable coercion.

function PromiseThen(onResolve, onReject) {
  var status = GET_PRIVATE(this, promiseStatusSymbol);
  if (IS_UNDEFINED(status)) {
    throw MakeTypeError(kNotAPromise, this);
  }

  var constructor = SpeciesConstructor(this, GlobalPromise);
  onResolve = IS_CALLABLE(onResolve) ? onResolve : PromiseIdResolveHandler;
  onReject = IS_CALLABLE(onReject) ? onReject : PromiseIdRejectHandler;
  var deferred = NewPromiseCapability(constructor);
  switch (status) {
    case 0:  // Pending
      GET_PRIVATE(this, promiseOnResolveSymbol).push(onResolve, deferred);
      GET_PRIVATE(this, promiseOnRejectSymbol).push(onReject, deferred);
      break;
    case +1:  // Resolved
      PromiseEnqueue(GET_PRIVATE(this, promiseValueSymbol),
                     [onResolve, deferred],
                     +1);
      break;
    case -1:  // Rejected
      if (!HAS_DEFINED_PRIVATE(this, promiseHasHandlerSymbol)) {
        // Promise has already been rejected, but had no handler.
        // Revoke previously triggered reject event.
        %PromiseRevokeReject(this);
      }
      PromiseEnqueue(GET_PRIVATE(this, promiseValueSymbol),
                     [onReject, deferred],
                     -1);
      break;
  }
  // Mark this promise as having handler.
  SET_PRIVATE(this, promiseHasHandlerSymbol, true);
  if (DEBUG_IS_ACTIVE) {
    %DebugPromiseEvent({ promise: deferred.promise, parentPromise: this });
  }
  return deferred.promise;
}

// Chain is left around for now as an alias for then
function PromiseChain(onResolve, onReject) {
  %IncrementUseCounter(kPromiseChain);
  return %_Call(PromiseThen, this, onResolve, onReject);
}

function PromiseCatch(onReject) {
  return this.then(UNDEFINED, onReject);
}

// Combinators.

function PromiseCast(x) {
  if (!IS_RECEIVER(this)) {
    throw MakeTypeError(kCalledOnNonObject, PromiseCast);
  }
  if (IsPromise(x) && x.constructor === this) return x;

  var promiseCapability = NewPromiseCapability(this);
  var resolveResult = %_Call(promiseCapability.resolve, UNDEFINED, x);
  return promiseCapability.promise;
}

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
  var queue = GET_PRIVATE(promise, promiseOnRejectSymbol);
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
  "reject", PromiseRejected,
  "all", PromiseAll,
  "race", PromiseRace,
  "resolve", PromiseCast
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
  "promise_reject", PromiseReject,
  "promise_resolve", PromiseResolve,
  "promise_then", PromiseThen,
]);

// This allows extras to create promises quickly without building extra
// resolve/reject closures, and allows them to later resolve and reject any
// promise without having to hold on to those closures forever.
utils.InstallFunctions(extrasUtils, 0, [
  "createPromise", PromiseCreate,
  "resolvePromise", PromiseResolve,
  "rejectPromise", PromiseReject
]);

// TODO(v8:4567): Allow experimental natives to remove function prototype
[PromiseChain, PromiseDeferred, PromiseResolved].forEach(
    fn => %FunctionRemovePrototype(fn));

utils.Export(function(to) {
  to.PromiseChain = PromiseChain;
  to.PromiseDeferred = PromiseDeferred;
  to.PromiseResolved = PromiseResolved;
});

})
