// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

async function runTests() {
  const child = new NodeInstance(['--inspect-brk=0'],
                                 `let c = 0;
                                  const interval = setInterval(() => {
                                   console.log(new Object());
                                   if (c++ === 10)
                                     clearInterval(interval);
                                 }, 10);`);
  const session = await child.connectInspectorSession();

  let stderrString = await child.nextStderrString();
  assert(stderrString.includes('Debugger listening on'), stderrString);
  stderrString = await child.nextStderrString();
  assert.strictEqual(stderrString,
                     'For help see https://nodejs.org/en/docs/inspector');
  stderrString = await child.nextStderrString();
  assert.strictEqual(stderrString, '');

  session.send([
    { 'method': 'Profiler.setSamplingInterval', 'params': { 'interval': 100 } },
    { 'method': 'Profiler.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
    // { 'method': 'Profiler.start' }
  ]);

  stderrString = await child.nextStderrString();
  assert.strictEqual(stderrString, 'Debugger attached.')
  stderrString = await child.nextStderrString();
  assert.strictEqual(stderrString, '');
  // stderrString = await child.nextStderrString();
  // assert.strictEqual(stderrString, 'Waiting for the debugger to disconnect...');

  // await session.send({ 'method': 'Profiler.stop' });
  // session.disconnect();
  // assert.strictEqual(0, (await child.expectShutdown()).exitCode);
  child.kill();
}

common.crashOnUnhandledRejection();
runTests();
