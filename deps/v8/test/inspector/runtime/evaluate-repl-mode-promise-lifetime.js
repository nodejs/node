// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-stress-incremental-marking

let {Protocol} = InspectorTest.start(
    'Tests the lifetime of Runtime.evaluate REPL promises.');

InspectorTest.runAsyncTestSuite([
  async function testPromiseIsKeptAlive() {
    const evaluation = Protocol.Runtime.evaluate({
      expression:
          'await new Promise(resolve => globalThis.resolve = resolve); 42',
      replMode: true,
    });

    await Protocol.HeapProfiler.collectGarbage();
    await Protocol.Runtime.evaluate({expression: 'resolve()'});

    InspectorTest.logMessage(await evaluation);
  },

  async function testObjectGroupReleaseMakesPromiseCollectible() {
    const evaluation = Protocol.Runtime.evaluate({
      expression: 'await new Promise(() => {})',
      objectGroup: 'evaluation',
      replMode: true,
    });

    await Protocol.Runtime.releaseObjectGroup({objectGroup: 'evaluation'});
    await Protocol.HeapProfiler.collectGarbage();

    InspectorTest.logMessage(await evaluation);
  },

  async function testContextDestructionDiscardsPromise() {
    const evaluation = Protocol.Runtime.evaluate({
      expression: 'await new Promise(() => {})',
      replMode: true,
    });

    await Protocol.Runtime.evaluate(
        {expression: 'inspector.fireContextDestroyed()'});

    InspectorTest.logMessage(await evaluation);
  },
]);
