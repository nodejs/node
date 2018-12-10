// Flags: --expose-internals
'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
common.skipIf32Bits();
const {
  NodeInstance,
  getDebuggerStatementIndices,
  firstBreakOffset: {
    eval: evalDebugBreakOffset
  },
  firstBreakIndex: {
    eval: evalDebugBreakIndex
  }
} = require('../common/inspector-helper.js');
const assert = require('assert');

const fn = `function runTest() {
  const p = Promise.resolve();
  p.then(function break1() { // lineNumber 3
    debugger;
  });
  p.then(function break2() { // lineNumber 6
    debugger;
  });
}
`;
const script = `${fn}\nrunTest();`;

const fnBreaks = getDebuggerStatementIndices(fn);
const scriptBreak1 = fnBreaks[0] + evalDebugBreakOffset;
const scriptBreak2 = fnBreaks[1] + evalDebugBreakOffset;

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

  await session.waitForBreakOnLine(evalDebugBreakIndex, '[eval]');
  await session.send({ 'method': 'Debugger.resume' });

  console.error('[test] Waiting for break1');
  debuggerPausedAt(await session.waitForBreakOnLine(scriptBreak1, '[eval]'),
                   'break1', `runTest:${fnBreaks[0]}`);

  await session.send({ 'method': 'Debugger.resume' });

  console.error('[test] Waiting for break2');
  debuggerPausedAt(await session.waitForBreakOnLine(scriptBreak2, '[eval]'),
                   'break2', `runTest:${fnBreaks[1]}`);

  await session.runToCompletion();
  assert.strictEqual((await instance.expectShutdown()).exitCode, 0);
}

function debuggerPausedAt(msg, functionName, previousTickLocation) {
  assert(
    !!msg.params.asyncStackTrace,
    `${Object.keys(msg.params)} contains "asyncStackTrace" property`);

  assert.strictEqual(msg.params.callFrames[0].functionName, functionName);
  assert.strictEqual(msg.params.asyncStackTrace.description, 'Promise.then');

  const frameLocations = msg.params.asyncStackTrace.callFrames.map(
    (frame) => `${frame.functionName}:${frame.lineNumber}`);
  assertArrayIncludes(frameLocations, previousTickLocation);
}

function assertArrayIncludes(actual, expected) {
  const expectedString = JSON.stringify(expected);
  const actualString = JSON.stringify(actual);
  assert(
    actual.includes(expected),
    `Expected ${actualString} to contain ${expectedString}.`);
}

runTests();
