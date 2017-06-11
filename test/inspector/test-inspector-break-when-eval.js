'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const helper = require('./inspector-helper.js');
const path = require('path');

const script = path.join(path.dirname(module.filename), 'global-function.js');


function setupExpectBreakOnLine(line, url, session) {
  return function(message) {
    if ('Debugger.paused' === message['method']) {
      const callFrame = message['params']['callFrames'][0];
      const location = callFrame['location'];
      assert.strictEqual(url, session.scriptUrlForId(location['scriptId']));
      assert.strictEqual(line, location['lineNumber']);
      return true;
    }
  };
}

function setupExpectConsoleOutputAndBreak(type, values) {
  if (!(values instanceof Array))
    values = [ values ];
  let consoleLog = false;
  function matchConsoleLog(message) {
    if ('Runtime.consoleAPICalled' === message['method']) {
      const params = message['params'];
      if (params['type'] === type) {
        let i = 0;
        for (const value of params['args']) {
          if (value['value'] !== values[i++])
            return false;
        }
        return i === values.length;
      }
    }
  }

  return function(message) {
    if (consoleLog)
      return message['method'] === 'Debugger.paused';
    consoleLog = matchConsoleLog(message);
    return false;
  };
}

function setupExpectContextDestroyed(id) {
  return function(message) {
    if ('Runtime.executionContextDestroyed' === message['method'])
      return message['params']['executionContextId'] === id;
  };
}

function setupDebugger(session) {
  console.log('[test]', 'Setting up a debugger');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': {'maxDepth': 0} },
    { 'method': 'Runtime.runIfWaitingForDebugger' },
  ];

  session
    .sendInspectorCommands(commands)
    .expectMessages((message) => 'Runtime.consoleAPICalled' === message.method);
}

function breakOnLine(session) {
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
  helper.markMessageNoResponse(commands[1]);
  session
    .sendInspectorCommands(commands)
    .expectMessages(setupExpectBreakOnLine(9, script, session));
}

function stepOverConsoleStatement(session) {
  console.log('[test]', 'Step over console statement and test output');
  session
    .sendInspectorCommands({ 'method': 'Debugger.stepOver' })
    .expectMessages(setupExpectConsoleOutputAndBreak('log', [0, 3]));
}

function testWaitsForFrontendDisconnect(session, harness) {
  console.log('[test]', 'Verify node waits for the frontend to disconnect');
  session.sendInspectorCommands({ 'method': 'Debugger.resume'})
    .expectMessages(setupExpectContextDestroyed(1))
    .expectStderrOutput('Waiting for the debugger to disconnect...')
    .disconnect(true);
}

function runTests(harness) {
  harness
    .runFrontendSession([
      setupDebugger,
      breakOnLine,
      stepOverConsoleStatement,
      testWaitsForFrontendDisconnect
    ]).expectShutDown(0);
}

helper.startNodeForInspectorTest(runTests,
                                 ['--inspect'],
                                 undefined,
                                 script);
