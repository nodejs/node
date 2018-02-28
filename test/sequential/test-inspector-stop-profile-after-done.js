// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

const stderrMessages = [];

async function runTests() {
  const child = new NodeInstance(['--inspect-brk=0'],
                                 `const interval = setInterval(() => {
                                    console.log(new Object());
                                  }, 140);
                                  process.stdin.on('data', (msg) => {
                                    if (msg.toString() === 'fhqwhgads') {
                                      clearInterval(interval);
                                      process.exit();
                                    }
                                  });`);

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
    { 'method': 'Profiler.setSamplingInterval', 'params': { 'interval': 1400 } },
    { 'method': 'Profiler.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);

  await child.nextStderrString();
  await child.nextStderrString();
  await child.nextStderrString();
  const stderrString = await child.nextStderrString();
  assert.strictEqual(stderrString, 'Debugger attached.');

  session.send({ 'method': 'Profiler.start' })
  setTimeout(() => { child._process.stdin.write('fhqwhgads'); }, 2800);
}

common.crashOnUnhandledRejection();
runTests();
