'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const inspector = require('inspector');
const path = require('path');

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
  const params = notification['params'];
  const callFrame = params['callFrames'][0];
  const scopeId = callFrame['scopeChain'][0]['object']['objectId'];
  checkScope(session, scopeId);
}

function testNoCrashWithExceptionInCallback() {
  // There is a deliberate exception in the callback
  const session = new inspector.Session();
  session.connect();
  const error = new Error('We expect this');
  assert.throws(() => {
    session.post('Console.enable', () => { throw error; });
  }, (e) => e === error);
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
    for (v of result['result']) {
      actual = v['value']['value'];
      expected = expects[v['name']][i];
      if (actual !== expected) {
        failures.push('Iteration ' + i + ' variable: ' + v['name'] +
                      ' expected: ' + expected + ' actual: ' + actual);
      }
    }
  };
  const session = new inspector.Session();
  session.connect();
  let secondSessionOpened = false;
  const secondSession = new inspector.Session();
  try {
    secondSession.connect();
    secondSessionOpened = true;
  } catch (error) {
    // expected as the session already exists
  }
  assert.strictEqual(secondSessionOpened, false);
  session.on('Debugger.paused',
             (notification) => debuggerPausedCallback(session, notification));
  let cbAsSecondArgCalled = false;
  assert.throws(() => {
    session.post('Debugger.enable', function() {}, function() {});
  }, TypeError);
  session.post('Debugger.enable', () => cbAsSecondArgCalled = true);
  session.post('Debugger.setBreakpointByUrl', {
    'lineNumber': 12,
    'url': path.resolve(__dirname, __filename),
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

testNoCrashWithExceptionInCallback();
testSampleDebugSession();
let breakpointHit = false;
scopeCallback = () => (breakpointHit = true);
debuggedFunction();
assert.strictEqual(breakpointHit, false);
testSampleDebugSession();
