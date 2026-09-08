'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

// An in-process session observes Debugger.paused while the process is paused,
// so its callback runs from the Isolate::RequestInterrupt handler that
// Debugger.pause schedules, where V8 asserts that JS is not executed.
const script = `
const { Session } = require('inspector');
const session = new Session();
let paused = false;
session.on('Debugger.paused', () => {
  paused = true;
});
session.connect();
session.post('Debugger.enable');
console.log('Ready');
// Spin so that the pause is requested while JS is on the stack, which is what
// makes V8 break from the interrupt handler instead of on the next call.
const deadline = Date.now() + ${common.platformTimeout(10000)};
while (!paused && Date.now() < deadline);
console.log(paused ? 'Notified' : 'Not notified');
`;

async function runTest() {
  const child = new NodeInstance(undefined, script);
  const session = await child.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send([
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
  ]);
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.send({ method: 'NodeRuntime.disable' });
  await session.waitForNotification('Debugger.paused', 'Break on start');
  await session.send({ 'method': 'Debugger.resume' });
  await session.waitForConsoleOutput('log', ['Ready']);

  await session.send({ 'method': 'Debugger.pause' });
  await session.waitForNotification('Debugger.paused', 'Paused');
  await session.send({ 'method': 'Debugger.resume' });
  await session.waitForConsoleOutput('log', ['Notified']);

  await session.waitForDisconnect();
  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
}

runTest().then(common.mustCall());
