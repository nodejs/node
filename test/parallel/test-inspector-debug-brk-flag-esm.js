'use strict';
const common = require('../common');

// Test that --inspect-brk pauses at the first executable line of an ESM entry point.

common.skipIfInspectorDisabled();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { NodeInstance } = require('../common/inspector-helper.js');

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
    { 'method': 'Runtime.runIfWaitingForDebugger' },
  ];

  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send(commands);
  await session.send({ method: 'NodeRuntime.disable' });
  // Static import declarations are not executable statements, so the break
  // happens at the first executable line (var t = 1) rather than line 0.
  await session.waitForBreakOnLine(2, session.scriptURL());
}

async function runTests() {
  const child = new NodeInstance(['--inspect-brk=0'], '', fixtures.path('es-modules', 'loop.mjs'));
  const session = await child.connectInspectorSession();

  await testBreakpointOnStart(session);
  await session.runToCompletion();

  assert.strictEqual((await child.expectShutdown()).exitCode, 55);
}

runTests().then(common.mustCall());
