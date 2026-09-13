'use strict';
const common = require('../common');

// Test that --inspect-brk pauses at the first executable line of an ESM entry
// point that uses top-level await (which takes the async evaluation path).

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
  // happens at the first executable line (await setTimeout(9), line index 4).
  await session.waitForBreakOnLine(4, session.scriptURL());
}

async function runTests() {
  const child = new NodeInstance(['--inspect-brk=0'], '',
                                 fixtures.path('es-modules', 'esm-top-level-await.mjs'));
  const session = await child.connectInspectorSession();

  await testBreakpointOnStart(session);
  await session.runToCompletion();

  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
}

runTests().then(common.mustCall());
