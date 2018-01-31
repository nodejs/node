// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { NodeInstance } = require('../common/inspector-helper.js');
const fixtures = require('../common/fixtures');

const script = fixtures.path('inspector-global-function.js');

async function setupDebugger(session) {
  console.log('[test]', 'Setting up a debugger');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': { 'maxDepth': 0 } },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
  ];
  session.send(commands);
  await session.waitForNotification('Runtime.consoleAPICalled');
}

async function breakOnLine(session) {
  console.log('[test]', 'Breaking in the code');
  const commands = [
    { 'method': 'Debugger.setBreakpointByUrl',
      'params': { 'lineNumber': 9,
                  'url': script,
                  'columnNumber': 0,
                  'condition': ''
      }
    },
    { 'method': 'Runtime.evaluate',
      'params': { 'expression': 'sum()',
                  'objectGroup': 'console',
                  'includeCommandLineAPI': true,
                  'silent': false,
                  'contextId': 1,
                  'returnByValue': false,
                  'generatePreview': true,
                  'userGesture': true,
                  'awaitPromise': false
      }
    }
  ];
  session.send(commands);
  await session.waitForBreakOnLine(9, script);
}

async function stepOverConsoleStatement(session) {
  console.log('[test]', 'Step over console statement and test output');
  session.send({ 'method': 'Debugger.stepOver' });
  await session.waitForConsoleOutput('log', [0, 3]);
  await session.waitForNotification('Debugger.paused');
}

async function runTests() {
  const child = new NodeInstance(['--inspect=0'], undefined, script);
  const session = await child.connectInspectorSession();
  await setupDebugger(session);
  await breakOnLine(session);
  await stepOverConsoleStatement(session);
  await session.runToCompletion();
  assert.strictEqual(0, (await child.expectShutdown()).exitCode);
}

common.crashOnUnhandledRejection();
runTests();
