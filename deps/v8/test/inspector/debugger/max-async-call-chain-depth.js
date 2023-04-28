// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(kozyatinskiy): fix or remove it later.
let {session, contextGroup, Protocol} = InspectorTest.start('Checks that we trim async call chains correctly.');

Protocol.Debugger.enable();
InspectorTest.log('set async chain depth to 8');
Protocol.Debugger.setAsyncCallStackDepth({maxDepth: 8});
InspectorTest.runAsyncTestSuite([
  async function testDebuggerPaused() {
    runWithAsyncChain(4, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChain(8, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChain(9, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChain(32, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testConsoleTrace() {
    Protocol.Runtime.enable();
    runWithAsyncChain(4, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChain(8, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChain(9, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChain(32, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testDebuggerPausedSetTimeout() {
    runWithAsyncChainSetTimeout(4, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChainSetTimeout(8, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChainSetTimeout(9, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithAsyncChainSetTimeout(32, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testConsoleTraceSetTimeout() {
    runWithAsyncChainSetTimeout(4, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChainSetTimeout(8, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChainSetTimeout(9, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithAsyncChainSetTimeout(32, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function testConsoleTraceWithEmptySync() {
    Protocol.Runtime.evaluate({
      expression: 'new Promise(resolve => setTimeout(resolve, 0)).then(() => console.trace(42))'
    });
    InspectorTest.logMessage((await Protocol.Runtime.onceConsoleAPICalled()).params.stackTrace);
  },

  async function testDebuggerPausedThenableJob() {
    runWithThenableJob(4, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithThenableJob(8, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithThenableJob(9, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();

    runWithThenableJob(32, 'debugger;');
    dumpAsyncChainLength(await Protocol.Debugger.oncePaused());
    await Protocol.Debugger.resume();
  },

  async function testConsoleTraceThenableJob() {
    runWithThenableJob(4, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithThenableJob(8, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithThenableJob(9, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());

    runWithThenableJob(32, 'console.trace(42);');
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  },

  async function twoConsoleAssert() {
    Protocol.Runtime.evaluate({
      expression: 'setTimeout(' +
        'setTimeout.bind(null, ' +
          'setTimeout.bind(null, () => { console.assert(); setTimeout(console.assert, 0) }, 0), 0), 0)'
    });
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
    dumpAsyncChainLength(await Protocol.Runtime.onceConsoleAPICalled());
  }
]);

function runWithAsyncChain(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let then = '.then(() => 1)';
  let pause = `.then(() => { ${source} })`;
  Protocol.Runtime.evaluate({
    expression: `Promise.resolve()${then.repeat(len - 1)}${pause}`
  });
}

function runWithAsyncChainSetTimeout(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let setTimeout = 'setTimeout(() => {';
  let suffix = '}, 0)';
  Protocol.Runtime.evaluate({
    expression: `${setTimeout.repeat(len)}${source}${suffix.repeat(len)}`
  });
}

function runWithThenableJob(len, source) {
  InspectorTest.log(`Run expression '${source}' with async chain len: ${len}`);
  let then = '.then(Promise.resolve.bind(Promise, 0))';
  let pause = `.then(() => { ${source} })`;
  Protocol.Runtime.evaluate({
    expression: `Promise.resolve()${then.repeat(len - 1)}${pause}`
  });
}

function dumpAsyncChainLength(message) {
  let stackTrace = message.params.asyncStackTrace || message.params.stackTrace.parent;
  let asyncChainCount = 0;
  while (stackTrace) {
    ++asyncChainCount;
    stackTrace = stackTrace.parent;
  }
  InspectorTest.log(`actual async chain len: ${asyncChainCount}`);
}
