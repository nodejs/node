// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

let {session, contextGroup, Protocol} = InspectorTest.start("Tests that Runtime.awaitPromise works.");

contextGroup.addScript(
`
var resolveCallback;
var rejectCallback;
function createPromise()
{
    return new Promise((resolve, reject) => { resolveCallback = resolve; rejectCallback = reject });
}

function resolvePromise()
{
    resolveCallback(239);
    resolveCallback = undefined;
    rejectCallback = undefined;
}

function rejectPromise()
{
    rejectCallback(239);
    resolveCallback = undefined;
    rejectCallback = undefined;
}

function rejectPromiseWithAnError()
{
    rejectCallback(new Error('MyError'));
    resolveCallback = undefined;
    rejectCallback = undefined;
}

function throwError()
{
   throw new Error('MyError');
}

//# sourceURL=test.js`);

Protocol.Debugger.enable()
  .then(() => Protocol.Debugger.setAsyncCallStackDepth({ maxDepth: 128 }))
  .then(() => testSuite());

function testSuite()
{
  InspectorTest.runTestSuite([
    function testResolvedPromise(next)
    {
      Protocol.Runtime.evaluate({ expression: "Promise.resolve(239)"})
        .then(result => Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId, returnByValue: false, generatePreview: true }))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());
    },

    function testRejectedPromise(next)
    {
      Protocol.Runtime.evaluate({ expression: "Promise.reject({ a : 1 })"})
        .then(result => Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId, returnByValue: true, generatePreview: false }))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());
    },

    function testRejectedPromiseWithStack(next)
    {
      Protocol.Runtime.evaluate({ expression: "createPromise()"})
        .then(result => scheduleRejectAndAwaitPromise(result))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());

      function scheduleRejectAndAwaitPromise(result)
      {
        var promise = Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId });
        Protocol.Runtime.evaluate({ expression: "rejectPromise()" });
        return promise;
      }
    },

    function testRejectedPromiseWithError(next)
    {
      Protocol.Runtime.evaluate({ expression: "createPromise()"})
        .then(result => scheduleRejectAndAwaitPromise(result))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());

      function scheduleRejectAndAwaitPromise(result)
      {
        var promise = Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId });
        Protocol.Runtime.evaluate({ expression: "rejectPromiseWithAnError()" });
        return promise;
      }
    },

    function testPendingPromise(next)
    {
      Protocol.Runtime.evaluate({ expression: "createPromise()"})
        .then(result => scheduleFulfillAndAwaitPromise(result))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());

      function scheduleFulfillAndAwaitPromise(result)
      {
        var promise = Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId });
        Protocol.Runtime.evaluate({ expression: "resolvePromise()" });
        return promise;
      }
    },

    function testResolvedWithoutArgsPromise(next)
    {
      Protocol.Runtime.evaluate({ expression: "Promise.resolve()"})
        .then(result => Protocol.Runtime.awaitPromise({ promiseObjectId: result.result.result.objectId, returnByValue: true, generatePreview: false }))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());
    }
  ]);
}
