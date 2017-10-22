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
}

function throwError()
{
  function foo() {
    throw new Error('MyError');
  }
  foo();
}

function throwSyntaxError()
{
  function foo() {
    eval('}');
  }
  foo();
}
`);

InspectorTest.runAsyncTestSuite([
  async function testResolvedPromise()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "Promise.resolve(239)",
      awaitPromise: true,
      generatePreview: true
    }));
  },

  async function testRejectedPromise()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "Promise.reject(239)",
      awaitPromise: true
    }));
  },

  async function testRejectedPromiseWithError()
  {
    Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "Promise.resolve().then(throwError)",
      awaitPromise: true
    }));
    await Protocol.Runtime.disable();
  },

  async function testRejectedPromiseWithSyntaxError()
  {
    Protocol.Runtime.enable();
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "Promise.resolve().then(throwSyntaxError)",
      awaitPromise: true
    }));
    await Protocol.Runtime.disable();
  },

  async function testPrimitiveValueInsteadOfPromise()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "true",
      awaitPromise: true
    }));
  },

  async function testObjectInsteadOfPromise()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "({})",
      awaitPromise: true,
      returnByValue: true
    }));
  },

  async function testPendingPromise()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "createPromiseAndScheduleResolve()",
      awaitPromise: true,
      returnByValue: true
    }));
  },

  async function testExceptionInEvaluate()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: "throw 239",
      awaitPromise: true
    }));
  },

  async function testThenableJob()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: '({then: resolve => resolve(42)})',
      awaitPromise: true}));
  },

  async function testLastEvaluatedResult()
  {
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: 'Promise.resolve(42)',
      awaitPromise: true,
      objectGroup: 'console'
    }));
    InspectorTest.logMessage(await Protocol.Runtime.evaluate({
      expression: '$_',
      includeCommandLineAPI: true
    }));
  },

  async function testRuntimeDisable()
  {
    await Protocol.Runtime.enable();
    Protocol.Runtime.evaluate({
      expression: 'new Promise(r1 => r = r1)',
      awaitPromise: true }).then(InspectorTest.logMessage);
    await Protocol.Runtime.disable();
    InspectorTest.log('Resolving promise..');
    await Protocol.Runtime.evaluate({expression: 'r({a:1})'});
    InspectorTest.log('Promise resolved');
  },

  async function testImmediatelyResolvedAfterAfterContextDestroyed()
  {
    Protocol.Runtime.evaluate({
      expression: 'new Promise(() => 42)',
      awaitPromise: true }).then(InspectorTest.logMessage);
    InspectorTest.log('Destroying context..');
    await Protocol.Runtime.evaluate({expression: 'inspector.fireContextDestroyed()'});
    InspectorTest.log('Context destroyed');
    InspectorTest.log('Triggering weak callback..');
    await Protocol.HeapProfiler.collectGarbage();
  }
]);
