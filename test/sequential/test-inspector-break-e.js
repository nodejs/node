// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const {
  NodeInstance,
  firstBreakIndex: {
    eval: evalDebugBreakIndex
  }
} = require('../common/inspector-helper.js');

async function runTests() {
  const instance = new NodeInstance(undefined, 'console.log(10)');
  const session = await instance.connectInspectorSession();
  await session.send([
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);
  await session.waitForBreakOnLine(evalDebugBreakIndex, '[eval]');
  await session.runToCompletion();
  assert.strictEqual((await instance.expectShutdown()).exitCode, 0);
}

runTests();
