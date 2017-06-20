'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const helper = require('./inspector-helper.js');

function setupExpectBreakInContext() {
  return function(message) {
    if ('Debugger.paused' === message['method']) {
      const callFrame = message['params']['callFrames'][0];
      assert.strictEqual(callFrame['functionName'], 'testfn');
      return true;
    }
  };
}

function testBreakpointInContext(session) {
  console.log('[test]',
              'Verifying debugger stops on start (--inspect-brk option)');
  const commands = [
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ];
  session
    .sendInspectorCommands(commands)
    .expectMessages(setupExpectBreakInContext());
}

function resume(session) {
  session
    .sendInspectorCommands({ 'method': 'Debugger.resume'})
    .disconnect(true);
}

function runTests(harness) {
  harness
    .runFrontendSession([
      testBreakpointInContext,
      resume,
    ]).expectShutDown(0);
}

const script = 'require("vm").runInNewContext(' +
               '"function testfn() {debugger};testfn();")';

helper.startNodeForInspectorTest(runTests, '--inspect-brk', script);
