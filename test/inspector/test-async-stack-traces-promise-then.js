'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
const helper = require('./inspector-helper');
const assert = require('assert');

const script = `runTest();
function runTest() {
  const p = Promise.resolve();
  p.then(function break1() { // lineNumber 3
    debugger;
  });
  p.then(function break2() { // lineNumber 6
    debugger;
  });
}
`;

helper.startNodeForInspectorTest(runTests,
                                 `--inspect-brk=${common.PORT}`,
                                 script);

function runTests(harness) {
  harness.runFrontendSession([
    helper.setupDebugger,
    (session) => {
      console.error('[test] Waiting for break1');
      session.expectMessages(debuggerPausedAt('break1', 'runTest:3'));
    },
    (session) => {
      console.error('[test] Waiting for break2');
      session
        .sendInspectorCommands({ method: 'Debugger.resume' })
        .expectMessages(debuggerPausedAt('break2', 'runTest:6'));
    },
    (session) => {
      console.log('[test] Disconnecting the debugging session');
      session.disconnect(true);
    }
  ]);
  // no need to kill the debugged process, it will quit naturally

  function debuggerPausedAt(functionName, previousTickLocation) {
    return function(msg) {
      if (msg.method !== 'Debugger.paused') return;

      assert(
        !!msg.params.asyncStackTrace,
        `${Object.keys(msg.params)} contains "asyncStackTrace" property`);

      assert.strictEqual(msg.params.callFrames[0].functionName, functionName);
      assert.strictEqual(msg.params.asyncStackTrace.description, 'PROMISE');

      const frameLocations = msg.params.asyncStackTrace.callFrames.map(
        (frame) => `${frame.functionName}:${frame.lineNumber}`);
      assertArrayIncludes(frameLocations, previousTickLocation);
      return true;
    };
  }

  function assertArrayIncludes(actual, expected) {
    const expectedString = JSON.stringify(expected);
    const actualString = JSON.stringify(actual);
    assert(
      actual.includes(expected),
      `Expected ${actualString} to contain ${expectedString}.`);
  }
}
