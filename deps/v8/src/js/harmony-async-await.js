// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils, extrasUtils) {

"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var AsyncFunctionNext;
var AsyncFunctionThrow;
var PromiseReject;
var PromiseResolve;
var PromiseThen;

utils.Import(function(from) {
  AsyncFunctionNext = from.AsyncFunctionNext;
  AsyncFunctionThrow = from.AsyncFunctionThrow;
  PromiseReject = from.PromiseCreateRejected;
  PromiseResolve = from.PromiseCreateResolved;
  PromiseThen = from.PromiseThen;
});

// -------------------------------------------------------------------

function AsyncFunctionAwait(generator, value) {
  return %_Call(
      PromiseThen, PromiseResolve(value),
      function(sentValue) {
        return %_Call(AsyncFunctionNext, generator, sentValue);
      },
      function(sentError) {
        return %_Call(AsyncFunctionThrow, generator, sentError);
      });
}

%InstallToContext([ "async_function_await", AsyncFunctionAwait ]);

})
