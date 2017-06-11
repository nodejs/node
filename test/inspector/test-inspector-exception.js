'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const helper = require('./inspector-helper.js');
const path = require('path');

const script = path.join(common.fixturesDir, 'throws_error.js');


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

function testBreakpointOnStart(session) {
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setPauseOnExceptions',
      'params': {'state': 'none'} },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': {'maxDepth': 0} },
    { 'method': 'Profiler.enable' },
    { 'method': 'Profiler.setSamplingInterval',
      'params': {'interval': 100} },
    { 'method': 'Debugger.setBlackboxPatterns',
      'params': {'patterns': []} },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ];

  session
    .sendInspectorCommands(commands)
    .expectMessages(setupExpectBreakOnLine(0, script, session));
}

function testWaitsForFrontendDisconnect(session, harness) {
  console.log('[test]', 'Verify node waits for the frontend to disconnect');
  session.sendInspectorCommands({ 'method': 'Debugger.resume'})
    .expectStderrOutput('Waiting for the debugger to disconnect...')
    .disconnect(true);
}

function runTests(harness) {
  harness
    .runFrontendSession([
      testBreakpointOnStart,
      testWaitsForFrontendDisconnect
    ]).expectShutDown(1);
}

helper.startNodeForInspectorTest(runTests,
                                 undefined,
                                 undefined,
                                 script);
