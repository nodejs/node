'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { NodeInstance } = require('./inspector-helper.js');
const path = require('path');

const script = path.join(common.fixturesDir, 'throws_error.js');

async function testBreakpointOnStart(session) {
  console.log('[test]',
              'Verifying debugger stops on start (--inspect-brk option)');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setPauseOnExceptions',
      'params': { 'state': 'none' } },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': { 'maxDepth': 0 } },
    { 'method': 'Profiler.enable' },
    { 'method': 'Profiler.setSamplingInterval',
      'params': { 'interval': 100 } },
    { 'method': 'Debugger.setBlackboxPatterns',
      'params': { 'patterns': [] } },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ];

  await session.send(commands);
  await session.waitForBreakOnLine(0, script);
}


async function runTest() {
  const child = new NodeInstance(undefined, undefined, script);
  const session = await child.connectInspectorSession();
  await testBreakpointOnStart(session);
  await session.runToCompletion();
  assert.strictEqual(1, (await child.expectShutdown()).exitCode);
}

common.crashOnUnhandledRejection();

runTest();
