// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const inspector = require('inspector');
const path = require('path');
const { pathToFileURL } = require('url');

// This test case will set a breakpoint 4 lines below
function debuggedFunction() {
  let i;
  let accum = 0;
  for (i = 0; i < 5; i++) {
    accum += i;
  }
  return accum;
}

let scopeCallback = null;

function checkScope(session, scopeId) {
  session.post('Runtime.getProperties', {
    'objectId': scopeId,
    'ownProperties': false,
    'accessorPropertiesOnly': false,
    'generatePreview': true
  }, scopeCallback);
}

function debuggerPausedCallback(session, notification) {
  const params = notification.params;
  const callFrame = params.callFrames[0];
  const scopeId = callFrame.scopeChain[0].object.objectId;
  checkScope(session, scopeId);
}

function waitForWarningSkipAsyncStackTraces(resolve) {
  process.once('warning', function(warning) {
    if (warning.code === 'INSPECTOR_ASYNC_STACK_TRACES_NOT_AVAILABLE') {
      waitForWarningSkipAsyncStackTraces(resolve);
    } else {
      resolve(warning);
    }
  });
}

async function testNoCrashWithExceptionInCallback() {
  // There is a deliberate exception in the callback
  const session = new inspector.Session();
  session.connect();
  const error = new Error('We expect this');
  console.log('Expecting warning to be emitted');
  const promise = new Promise(waitForWarningSkipAsyncStackTraces);
  session.post('Console.enable', () => { throw error; });
  assert.strictEqual(await promise, error);
  session.disconnect();
}

function testSampleDebugSession() {
  let cur = 0;
  const failures = [];
  const expects = {
    i: [0, 1, 2, 3, 4],
    accum: [0, 0, 1, 3, 6]
  };
  scopeCallback = function(error, result) {
    const i = cur++;
    let v, actual, expected;
    for (v of result.result) {
      actual = v.value.value;
      expected = expects[v.name][i];
      if (actual !== expected) {
        failures.push(`Iteration ${i} variable: ${v.name} ` +
          `expected: ${expected} actual: ${actual}`);
      }
    }
  };
  const session = new inspector.Session();
  session.connect();
  session.on('Debugger.paused',
             (notification) => debuggerPausedCallback(session, notification));
  let cbAsSecondArgCalled = false;
  assert.throws(() => {
    session.post('Debugger.enable', function() {}, function() {});
  }, TypeError);
  session.post('Debugger.enable', () => cbAsSecondArgCalled = true);
  session.post('Debugger.setBreakpointByUrl', {
    'lineNumber': 14,
    'url': pathToFileURL(path.resolve(__dirname, __filename)).toString(),
    'columnNumber': 0,
    'condition': ''
  });

  debuggedFunction();
  assert.deepStrictEqual(cbAsSecondArgCalled, true);
  assert.deepStrictEqual(failures, []);
  assert.strictEqual(cur, 5);
  scopeCallback = null;
  session.disconnect();
  assert.throws(() => session.post('Debugger.enable'), (e) => !!e);
}

async function testNoCrashConsoleLogBeforeThrow() {
  const session = new inspector.Session();
  session.connect();
  let attempt = 1;
  process.on('warning', common.mustCall(3));
  session.on('inspectorNotification', () => {
    if (attempt++ > 3)
      return;
    console.log('console.log in handler');
    throw new Error('Exception in handler');
  });
  session.post('Runtime.enable');
  console.log('Did not crash');
  session.disconnect();
}

async function doTests() {
  await testNoCrashWithExceptionInCallback();
  testSampleDebugSession();
  let breakpointHit = false;
  scopeCallback = () => (breakpointHit = true);
  debuggedFunction();
  assert.strictEqual(breakpointHit, false);
  testSampleDebugSession();
  await testNoCrashConsoleLogBeforeThrow();
}

doTests();
