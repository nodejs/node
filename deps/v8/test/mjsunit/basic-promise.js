// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// We have to patch mjsunit because normal assertion failures just throw
// exceptions which are swallowed in a then clause.
failWithMessage = (msg) => %AbortJS(msg);

function newPromise() {
  var outerResolve;
  var outerReject;
  let promise = new Promise((resolve, reject) => {
    outerResolve = resolve;
    outerReject = reject;
  });
  Promise.resolve(promise);
  return {
    resolve: outerResolve,
    reject: outerReject,
    then: (f, g) => promise.then(f, g)
  };
}

(function ResolveOK() {
  let promise = newPromise();
  promise.then(msg => {print("resolved: " + msg); assertEquals("ok", msg); },
               ex => {print("rejected: " + ex); %AbortJS("" + ex); });

  promise.resolve("ok");
  promise.reject(11); // ignored
})();

(function RejectOK() {
  let promise = newPromise();
  promise.then(msg => {print("resolved: " + msg); %AbortJS("fail"); },
               ex => {print("rejected: " + ex); assertEquals(42, ex); });

  promise.reject(42);
  promise.resolve("fail"); // ignored
})();
