// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// If a Promise's then method is overridden, that should be respected
// even if the promise is already resolved. x's resolution function is
// only called by Promise.resolve(); there shouldn't be a resolution
// check before when calling x.then.


// Async assert framework copied from mjsunit/es6/promises.js

var asyncAssertsExpected = 0;

function assertAsyncRan() { ++asyncAssertsExpected }

function assertLater(f, name) {
  assertFalse(f()); // should not be true synchronously
  ++asyncAssertsExpected;
  var iterations = 0;
  function runAssertion() {
    if (f()) {
      print(name, "succeeded");
      --asyncAssertsExpected;
    } else if (iterations++ < 10) {
      %EnqueueMicrotask(runAssertion);
    } else {
      %AbortJS(name + " FAILED!");
    }
  }
  %EnqueueMicrotask(runAssertion);
}

function assertAsyncDone(iteration) {
  var iteration = iteration || 0;
  %EnqueueMicrotask(function() {
    if (asyncAssertsExpected === 0)
      assertAsync(true, "all")
    else if (iteration > 10)  // Shouldn't take more.
      assertAsync(false, "all... " + asyncAssertsExpected)
    else
      assertAsyncDone(iteration + 1)
  });
}

// End async assert framework

var y;
var x = Promise.resolve();
x.then = () => { y = true; }
Promise.resolve().then(() => x);
assertLater(() => y === true, "y === true");

assertAsyncDone();
