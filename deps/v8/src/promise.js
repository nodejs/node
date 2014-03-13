// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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


"use strict";

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Object = global.Object
// var $WeakMap = global.WeakMap


var $Promise = Promise;


//-------------------------------------------------------------------

// Core functionality.

// Event queue format: [(value, [(handler, deferred)*])*]
// I.e., a list of value/tasks pairs, where the value is a resolution value or
// rejection reason, and the tasks are a respective list of handler/deferred
// pairs waiting for notification of this value. Each handler is an onResolve or
// onReject function provided to the same call of 'chain' that produced the
// associated deferred.
var promiseEvents = new InternalArray;

// Status values: 0 = pending, +1 = resolved, -1 = rejected
var promiseStatus = NEW_PRIVATE("Promise#status");
var promiseValue = NEW_PRIVATE("Promise#value");
var promiseOnResolve = NEW_PRIVATE("Promise#onResolve");
var promiseOnReject = NEW_PRIVATE("Promise#onReject");
var promiseRaw = NEW_PRIVATE("Promise#raw");

function IsPromise(x) {
  return IS_SPEC_OBJECT(x) && %HasLocalProperty(x, promiseStatus);
}

function Promise(resolver) {
  if (resolver === promiseRaw) return;
  if (!%_IsConstructCall()) throw MakeTypeError('not_a_promise', [this]);
  if (typeof resolver !== 'function')
    throw MakeTypeError('resolver_not_a_function', [resolver]);
  var promise = PromiseInit(this);
  try {
    resolver(function(x) { PromiseResolve(promise, x) },
             function(r) { PromiseReject(promise, r) });
  } catch (e) {
    PromiseReject(promise, e);
  }
}

function PromiseSet(promise, status, value, onResolve, onReject) {
  SET_PRIVATE(promise, promiseStatus, status);
  SET_PRIVATE(promise, promiseValue, value);
  SET_PRIVATE(promise, promiseOnResolve, onResolve);
  SET_PRIVATE(promise, promiseOnReject, onReject);
  return promise;
}

function PromiseInit(promise) {
  return PromiseSet(promise, 0, UNDEFINED, new InternalArray, new InternalArray)
}

function PromiseDone(promise, status, value, promiseQueue) {
  if (GET_PRIVATE(promise, promiseStatus) === 0) {
    PromiseEnqueue(value, GET_PRIVATE(promise, promiseQueue));
    PromiseSet(promise, status, value);
  }
}

function PromiseResolve(promise, x) {
  PromiseDone(promise, +1, x, promiseOnResolve)
}

function PromiseReject(promise, r) {
  PromiseDone(promise, -1, r, promiseOnReject)
}


// Convenience.

function PromiseDeferred() {
  if (this === $Promise) {
    // Optimized case, avoid extra closure.
    var promise = PromiseInit(new Promise(promiseRaw));
    return {
      promise: promise,
      resolve: function(x) { PromiseResolve(promise, x) },
      reject: function(r) { PromiseReject(promise, r) }
    };
  } else {
    var result = {};
    result.promise = new this(function(resolve, reject) {
      result.resolve = resolve;
      result.reject = reject;
    })
    return result;
  }
}

function PromiseResolved(x) {
  if (this === $Promise) {
    // Optimized case, avoid extra closure.
    return PromiseSet(new Promise(promiseRaw), +1, x);
  } else {
    return new this(function(resolve, reject) { resolve(x) });
  }
}

function PromiseRejected(r) {
  if (this === $Promise) {
    // Optimized case, avoid extra closure.
    return PromiseSet(new Promise(promiseRaw), -1, r);
  } else {
    return new this(function(resolve, reject) { reject(r) });
  }
}


// Simple chaining.

function PromiseIdResolveHandler(x) { return x }
function PromiseIdRejectHandler(r) { throw r }

function PromiseChain(onResolve, onReject) {  // a.k.a.  flatMap
  onResolve = IS_UNDEFINED(onResolve) ? PromiseIdResolveHandler : onResolve;
  onReject = IS_UNDEFINED(onReject) ? PromiseIdRejectHandler : onReject;
  var deferred = %_CallFunction(this.constructor, PromiseDeferred);
  switch (GET_PRIVATE(this, promiseStatus)) {
    case UNDEFINED:
      throw MakeTypeError('not_a_promise', [this]);
    case 0:  // Pending
      GET_PRIVATE(this, promiseOnResolve).push(onResolve, deferred);
      GET_PRIVATE(this, promiseOnReject).push(onReject, deferred);
      break;
    case +1:  // Resolved
      PromiseEnqueue(GET_PRIVATE(this, promiseValue), [onResolve, deferred]);
      break;
    case -1:  // Rejected
      PromiseEnqueue(GET_PRIVATE(this, promiseValue), [onReject, deferred]);
      break;
  }
  return deferred.promise;
}

function PromiseCatch(onReject) {
  return this.chain(UNDEFINED, onReject);
}

function PromiseEnqueue(value, tasks) {
  GetMicrotaskQueue().push(function() {
    for (var i = 0; i < tasks.length; i += 2) {
      PromiseHandle(value, tasks[i], tasks[i + 1])
    }
  });

  %SetMicrotaskPending(true);
}

function PromiseHandle(value, handler, deferred) {
  try {
    var result = handler(value);
    if (result === deferred.promise)
      throw MakeTypeError('promise_cyclic', [result]);
    else if (IsPromise(result))
      result.chain(deferred.resolve, deferred.reject);
    else
      deferred.resolve(result);
  } catch(e) {
    // TODO(rossberg): perhaps log uncaught exceptions below.
    try { deferred.reject(e) } catch(e) {}
  }
}


// Multi-unwrapped chaining with thenable coercion.

function PromiseThen(onResolve, onReject) {
  onResolve = IS_UNDEFINED(onResolve) ? PromiseIdResolveHandler : onResolve;
  var that = this;
  var constructor = this.constructor;
  return this.chain(
    function(x) {
      x = PromiseCoerce(constructor, x);
      return x === that ? onReject(MakeTypeError('promise_cyclic', [x])) :
             IsPromise(x) ? x.then(onResolve, onReject) : onResolve(x);
    },
    onReject
  );
}

PromiseCoerce.table = new $WeakMap;

function PromiseCoerce(constructor, x) {
  if (!(IsPromise(x) || IS_NULL_OR_UNDEFINED(x))) {
    var then = x.then;
    if (typeof then === 'function') {
      if (PromiseCoerce.table.has(x)) {
        return PromiseCoerce.table.get(x);
      } else {
        var deferred = %_CallFunction(constructor, PromiseDeferred);
        PromiseCoerce.table.set(x, deferred.promise);
        try {
          %_CallFunction(x, deferred.resolve, deferred.reject, then);
        } catch(e) {
          deferred.reject(e);
        }
        return deferred.promise;
      }
    }
  }
  return x;
}


// Combinators.

function PromiseCast(x) {
  // TODO(rossberg): cannot do better until we support @@create.
  return IsPromise(x) ? x : this.resolve(x);
}

function PromiseAll(values) {
  var deferred = %_CallFunction(this, PromiseDeferred);
  var resolutions = [];
  try {
    var count = values.length;
    if (count === 0) {
      deferred.resolve(resolutions);
    } else {
      for (var i = 0; i < values.length; ++i) {
        this.cast(values[i]).chain(
          function(i, x) {
            resolutions[i] = x;
            if (--count === 0) deferred.resolve(resolutions);
          }.bind(UNDEFINED, i),  // TODO(rossberg): use let loop once available
          function(r) { deferred.reject(r) }
        );
      }
    }
  } catch (e) {
    deferred.reject(e)
  }
  return deferred.promise;
}

function PromiseOne(values) {
  var deferred = %_CallFunction(this, PromiseDeferred);
  try {
    for (var i = 0; i < values.length; ++i) {
      this.cast(values[i]).chain(
        function(x) { deferred.resolve(x) },
        function(r) { deferred.reject(r) }
      );
    }
  } catch (e) {
    deferred.reject(e)
  }
  return deferred.promise;
}

//-------------------------------------------------------------------

function SetUpPromise() {
  %CheckIsBootstrapping()
  var global_receiver = %GlobalReceiver(global);
  global_receiver.Promise = $Promise;
  InstallFunctions($Promise, DONT_ENUM, [
    "defer", PromiseDeferred,
    "resolve", PromiseResolved,
    "reject", PromiseRejected,
    "all", PromiseAll,
    "race", PromiseOne,
    "cast", PromiseCast
  ]);
  InstallFunctions($Promise.prototype, DONT_ENUM, [
    "chain", PromiseChain,
    "then", PromiseThen,
    "catch", PromiseCatch
  ]);
}

SetUpPromise();
