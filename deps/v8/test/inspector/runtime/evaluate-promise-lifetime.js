// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {Protocol} = InspectorTest.start(
    'Tests the lifetime of pending Runtime.evaluate requests.');

const evaluationModes = [
  {
    name: 'replMode',
    arguments: {replMode: true},
    expression:
        'await new Promise(resolve => globalThis.resolve = resolve); 42',
    resolveExpression: 'resolve()',
    pendingExpression: 'await new Promise(() => {})',
  },
  {
    name: 'awaitPromise',
    arguments: {awaitPromise: true},
    expression: `(() => {
      let resolve;
      const promise = new Promise(r => resolve = r);
      promise.resolve = resolve;
      globalThis.weak = new WeakRef(promise);
      return promise;
    })()`,
    resolveExpression: 'weak.deref().resolve(42)',
    pendingExpression: 'new Promise(() => {})',
  },
];

function evaluate(Protocol, mode, expression, extraArguments = {}) {
  return Protocol.Runtime.evaluate(
      {...mode.arguments, ...extraArguments, expression});
}

InspectorTest.runAsyncTestSuite([
  async function testPromiseIsKeptAlive() {
    for (const mode of evaluationModes) {
      InspectorTest.log(`Using ${mode.name}:`);
      const evaluation = evaluate(Protocol, mode, mode.expression);

      await Protocol.HeapProfiler.collectGarbage();
      await Protocol.Runtime.evaluate({expression: mode.resolveExpression});

      InspectorTest.logMessage(await evaluation);
    }
  },

  async function testObjectGroupReleaseMakesPromiseCollectible() {
    for (const mode of evaluationModes) {
      InspectorTest.log(`Using ${mode.name}:`);
      const evaluation = evaluate(
          Protocol, mode, mode.pendingExpression,
          {objectGroup: 'evaluation'});

      await Protocol.Runtime.releaseObjectGroup({objectGroup: 'evaluation'});
      await Protocol.HeapProfiler.collectGarbage();

      InspectorTest.logMessage(await evaluation);
    }
  },

  async function testContextDestructionDiscardsPromise() {
    for (const mode of evaluationModes) {
      InspectorTest.log(`Using ${mode.name}:`);
      const contextGroup = new InspectorTest.ContextGroup();
      const session = contextGroup.connect();
      const evaluation = evaluate(
          session.Protocol, mode, mode.pendingExpression);

      await session.Protocol.Runtime.evaluate(
          {expression: 'inspector.fireContextDestroyed()'});

      InspectorTest.logMessage(await evaluation);
      session.disconnect();
    }
  },

  async function testSessionDestructionMakesPromiseCollectible() {
    const contextGroup = new InspectorTest.ContextGroup();
    const session1 = contextGroup.connect();
    const session2 = contextGroup.connect();
    session1.Protocol.Runtime.evaluate({
      expression: evaluationModes[1].expression,
      awaitPromise: true,
    });

    await session2.Protocol.HeapProfiler.collectGarbage();
    let result = await session2.Protocol.Runtime.evaluate(
        {expression: 'weak.deref() !== undefined'});
    InspectorTest.log(
        `Promise is alive before disconnect: ${result.result.result.value}`);

    session1.disconnect();
    await session2.Protocol.HeapProfiler.collectGarbage();
    result = await session2.Protocol.Runtime.evaluate(
        {expression: 'weak.deref() !== undefined'});
    InspectorTest.log(
        `Promise is alive after disconnect: ${result.result.result.value}`);
    session2.disconnect();
  },
]);
