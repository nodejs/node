// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --expose_gc

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
    },

    function testGarbageCollectedPromise(next)
    {
      Protocol.Runtime.evaluate({ expression: "new Promise(() => undefined)" })
        .then(result => scheduleGCAndawaitPromise(result))
        .then(result => InspectorTest.logMessage(result))
        .then(() => next());

      function scheduleGCAndawaitPromise(result)
      {
        var objectId = result.result.result.objectId;
        var promise = Protocol.Runtime.awaitPromise({ promiseObjectId: objectId });
        gcPromise(objectId);
        return promise;
      }

      function gcPromise(objectId)
      {
        Protocol.Runtime.releaseObject({ objectId: objectId})
          .then(() => Protocol.Runtime.evaluate({ expression: "gc()" }));
      }
    }
  ]);
}
