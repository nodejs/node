'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { mainScriptPath,
        NodeInstance } = require('./inspector-helper.js');

async function testBreakpointOnStart(session) {
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

  session.send(commands);
  await session.waitForBreakOnLine(0, mainScriptPath);
}

async function runTests() {
  const child = new NodeInstance(['--inspect', '--debug-brk']);
  const session = await child.connectInspectorSession();

  await testBreakpointOnStart(session);
  await session.runToCompletion();

  assert.strictEqual(55, (await child.expectShutdown()).exitCode);
}

common.crashOnUnhandledRejection();
runTests();
