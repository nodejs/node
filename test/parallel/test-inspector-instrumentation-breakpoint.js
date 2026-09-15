'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert/strict');

const fixtures = require('../common/fixtures');
const { NodeInstance } = require('../common/inspector-helper');

async function testBreakpointBeforeScriptExecution(session) {
  console.log(
    '[test]',
    'Verifying debugger stops on start of each script ' +
      '(Debugger.setInstrumentationBreakpoint with beforeScriptExecution)',
  );

  const commands = [
    { method: 'Runtime.enable' },
    { method: 'Debugger.enable' },
    {
      method: 'Debugger.setInstrumentationBreakpoint',
      params: { instrumentation: 'beforeScriptExecution' },
    },
    { method: 'Runtime.runIfWaitingForDebugger' },
  ];

  await session.send(commands);

  const mainURL = new URL('main.js', session.scriptURL()).href;

  // Break on start.
  await session.waitForBreakOnLine(3, 'node:internal/main/run_main_module');
  await session.send([{ method: 'Debugger.resume' }]);

  // Script loaded.
  await session.waitForBreakOnLine(0, mainURL);
  await session.send([{ method: 'Debugger.resume' }]);

  // Dependency loaded.
  await session.waitForBreakOnLine(0, mainURL);
}

async function runTest() {
  const main = fixtures.path(
    'inspector-instrumentation-breakpoint',
    'main.js',
  );

  const child = new NodeInstance(['--inspect-brk=0'], '', main);
  const session = await child.connectInspectorSession();

  await testBreakpointBeforeScriptExecution(session);
  await session.runToCompletion();

  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
}

runTest();
