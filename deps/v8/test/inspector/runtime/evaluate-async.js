// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.evaluate works with awaitPromise flag.");

contextGroup.addScript(`
function createPromiseAndScheduleResolve()
{
  var resolveCallback;
  var promise = new Promise((resolve) => resolveCallback = resolve);
  setTimeout(resolveCallback.bind(null, { a : 239 }), 0);
  return promise;
}`);

InspectorTest.runTestSuite([
  function testResolvedPromise(next)
  {
    Protocol.Runtime.evaluate({ expression: "Promise.resolve(239)", awaitPromise: true, generatePreview: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testRejectedPromise(next)
  {
    Protocol.Runtime.evaluate({ expression: "Promise.reject(239)", awaitPromise: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testPrimitiveValueInsteadOfPromise(next)
  {
    Protocol.Runtime.evaluate({ expression: "true", awaitPromise: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testObjectInsteadOfPromise(next)
  {
    Protocol.Runtime.evaluate({ expression: "({})", awaitPromise: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testPendingPromise(next)
  {
    Protocol.Runtime.evaluate({ expression: "createPromiseAndScheduleResolve()", awaitPromise: true, returnByValue: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  },

  function testExceptionInEvaluate(next)
  {
    Protocol.Runtime.evaluate({ expression: "throw 239", awaitPromise: true })
      .then(result => InspectorTest.logMessage(result))
      .then(() => next());
  }
]);
