// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we drop old async call chains.');

Protocol.Debugger.enable();
Protocol.Runtime.enable();
InspectorTest.runAsyncTestSuite([
  async function testInfrastructure() {
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});
    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainPromise(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainPromise(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainPromise(5, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainSetTimeout(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainSetTimeout(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(1024);
    runWithAsyncChainSetTimeout(5, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testZeroLimit() {
    const limit = 0;
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testOneLimit() {
    const limit = 1;
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testTwoLimit() {
    const limit = 2;
    Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainPromise(3, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(1, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(2, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    await setMaxAsyncTaskStacks(limit);
    runWithAsyncChainSetTimeout(3, 'console.trace(42)');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testMoreThanTwoLimit() {
    for (let limit = 3; limit <= 7; ++limit) {
      Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 128});

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainPromise(1, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainPromise(2, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainPromise(3, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainSetTimeout(1, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainSetTimeout(2, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

      await setMaxAsyncTaskStacks(limit);
      runWithAsyncChainSetTimeout(3, 'console.trace(42)');
      dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
    }
  },
]);

function runWithAsyncChainPromise(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let then = '.then(() => 1)';
  let pause = `.then(() => { ${source} })`;
  Protocol.Runtime.evaluate({
    expression: `Promise.resolve()${then.repeat(len - 1)}${pause}`
  });
}

function runWithAsyncChainSetTimeout(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let setTimeoutPrefix = '() => setTimeout(';
  let setTimeoutSuffix = ', 0)';
  Protocol.Runtime.evaluate({
    expression: `setTimeout(${setTimeoutPrefix.repeat(len - 1)}'${source}'${setTimeoutSuffix.repeat(len - 1)}, 0)`
  });
}

function dumpAsyncChainLength(message) {
  let stackTrace = message.params.asyncStackTrace || message.params.stackTrace.parent;
  let asyncChainCount = 0;
  while (stackTrace) {
    ++asyncChainCount;
    stackTrace = stackTrace.parent;
  }
  InspectorTest.log(`actual async chain len: ${asyncChainCount}\n`);
}

async function setMaxAsyncTaskStacks(max) {
  let expression = `inspector.setMaxAsyncTaskStacks(${max})`;
  InspectorTest.log(expression);
  await Protocol.Runtime.evaluate({expression});
}
