'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
common.crashOnUnhandledRejection();
const { NodeInstance } = require('../inspector/inspector-helper.js');
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

async function waitForInitialSetup(session) {
  console.error('[test]', 'Waiting for initial setup');
  await session.waitForBreakOnLine(13, '[eval]');
}

async function setupOwnInterval(session) {
  console.error('[test]', 'Setting up our own interval');
  await session.send([
    { 'method': 'Runtime.evaluate',
      'params': { expression: 'setupIntervalWithBreak()' } },
    { 'method': 'Debugger.resume' }
  ]);
}

async function checkAsyncStackTrace(session) {
  console.error('[test]', 'Verify basic properties of asyncStackTrace');
  const paused = await session.waitForBreakOnLine(20, '[eval]');
  assert(paused.params.asyncStackTrace,
         `${Object.keys(paused.params)} contains "asyncStackTrace" property`);
  assert(paused.params.asyncStackTrace.description, 'Timeout');
  assert(paused.params.asyncStackTrace.callFrames
           .some((frame) => frame.functionName === 'setupIntervalWithBreak'));
}

async function runTests() {
  const instance = await NodeInstance.startViaSignal(script);
  const session = await instance.connectInspectorSession();
  await session.send([
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
    { 'method': 'Debugger.setAsyncCallStackDepth',
      'params': { 'maxDepth': 10 } },
    { 'method': 'Debugger.setBlackboxPatterns',
      'params': { 'patterns': [] } },
    { 'method': 'Runtime.runIfWaitingForDebugger' }
  ]);

  await waitForInitialSetup(session);
  await setupOwnInterval(session);
  await checkAsyncStackTrace(session);

  console.error('[test]', 'Stopping child instance');
  session.disconnect();
  instance.kill();
}

runTests();
