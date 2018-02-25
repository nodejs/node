// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

const stderrMessages = [];

async function runTests() {
  const child = new NodeInstance(['--inspect-brk=0'],
                                 `let c = 0;
                                  const interval = setInterval(() => {
                                   console.log(new Object());
                                   if (c++ === 10)
                                     clearInterval(interval);
                                 }, 10);`);

  child._process.stderr.on('data', async (data) => {
    const message = data.toString();
    stderrMessages.push(message);
    if (message.trim() === 'Waiting for the debugger to disconnect...') {
      await session.send({ 'method': 'Profiler.stop' });
      session.disconnect();
      assert.strictEqual(0, (await child.expectShutdown()).exitCode);
    }
  })
  const session = await child.connectInspectorSession();

  session.send([
    { 'method': 'Profiler.setSamplingInterval', 'params': { 'interval': 100 } },
    { 'method': 'Profiler.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
    { 'method': 'Profiler.start' }
  ]);  
}

common.crashOnUnhandledRejection();
runTests();
