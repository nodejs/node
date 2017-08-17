'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
common.crashOnUnhandledRejection();
const { NodeInstance } = require('./inspector-helper');
const assert = require('assert');

const script = 'setInterval(() => { debugger; }, 50);';

async function checkAsyncStackTrace(session) {
  console.error('[test]', 'Verify basic properties of asyncStackTrace');
  const paused = await session.waitForBreakOnLine(0, '[eval]');
  assert(paused.params.asyncStackTrace,
         `${Object.keys(paused.params)} contains "asyncStackTrace" property`);
  assert(paused.params.asyncStackTrace.description, 'Timeout');
  assert(paused.params.asyncStackTrace.callFrames
           .some((frame) => frame.functionName === 'Module._compile'));
}

async function runTests() {
  const instance = new NodeInstance(undefined, script);
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

  await checkAsyncStackTrace(session);

  console.error('[test]', 'Stopping child instance');
  session.disconnect();
  instance.kill();
}

runTests();
