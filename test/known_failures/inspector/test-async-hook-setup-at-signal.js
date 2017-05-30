'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
const helper = require('./inspector-helper');
const assert = require('assert');

const script = `
process._rawDebug('Waiting until a signal enables the inspector...');
let waiting = setInterval(setupIntervalIfDebugged, 50);

function setupIntervalIfDebugged() {
  if (!process.binding('inspector').isEnabled()) return;
  clearInterval(waiting);
  // It seems that AsyncTask tracking engine may not be initialised yet
  // when isEnabled() flag changes to true, therefore we need to introduce
  // a small delay. Using a timeout would be brittle and it would unnecesarily
  // slow down the test on most machines.
  // Triggering a debugger break is a faster and more reliable way.
  process._rawDebug('Signal received, waiting for debugger setup');
  waiting = setInterval(() => { debugger; }, 50);
}

// This function is called by the inspector client (session)
function setupIntervalWithBreak() {
  clearInterval(waiting);
  process._rawDebug('Debugger ready, setting up interval with a break');
  setInterval(() => { debugger; }, 50);
}
`;

helper.startNodeAndDebuggerViaSignal(runTests, script);

function runTests(harness) {
  harness.runFrontendSession([
    helper.setupDebugger,
    waitForFirstBreak,
    continueAndAssertPausedMessage,
    (session) => session.disconnect(true),
  ]);

  harness.kill();

  function waitForFirstBreak(session) {
    session.expectMessages((msg) => msg.method === 'Debugger.paused');
  }

  function continueAndAssertPausedMessage(session) {
    session.sendInspectorCommands([
      {
        method: 'Runtime.evaluate',
        params: { expression: 'setupIntervalWithBreak()' },
      },
      {
        method: 'Debugger.resume',
      }]);

    session.expectMessages((msg) => {
      if (msg.method !== 'Debugger.paused') return;
      assert(
        !!msg.params.asyncStackTrace,
        `${Object.keys(msg.params)} contains "asyncStackTrace" property`);
      return true;
    });
  }
}
