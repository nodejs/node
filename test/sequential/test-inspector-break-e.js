// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');

async function runTests() {
  const instance = new NodeInstance(undefined, 'console.log(10)');
  const session = await instance.connectInspectorSession();
  await session.send([
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);
  await session.waitForBreakOnLine(2, '[eval]');
  await session.runToCompletion();
  assert.strictEqual(0, (await instance.expectShutdown()).exitCode);
}

runTests();
